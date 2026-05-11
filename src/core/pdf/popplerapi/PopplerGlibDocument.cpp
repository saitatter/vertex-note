#include "PopplerGlibDocument.h"

#include <limits>
#include <memory>    // for make_shared, unique_ptr
#include <optional>  // for optional

#include <poppler-document.h>  // for poppler_document_get_n_...
#include <poppler/cpp/poppler-document.h>

#include "util/PathUtil.h"  // for toUri
#include "util/StringUtils.h"

#include "PopplerGlibPage.h"                  // for PopplerGlibPage
#include "PopplerGlibPageBookmarkIterator.h"  // for PopplerGlibPageBookmark...
#include "filesystem.h"                       // for path

class PdfBookmarkIterator;

namespace {

auto adoptRenderDocument(poppler::document* document) -> std::shared_ptr<poppler::document> {
    return std::shared_ptr<poppler::document>(document, [](poppler::document* ptr) { delete ptr; });
}

auto makeRenderDocumentFromFile(const fs::path& file, const std::string& password)
        -> std::shared_ptr<poppler::document> {
    const auto fileName = file.generic_u8string();
    return adoptRenderDocument(poppler::document::load_from_file(std::string{char_cast(fileName)}, std::string{},
                                                                 password));
}

auto makeRenderDocumentFromData(const std::shared_ptr<std::string>& data, const std::string& password)
        -> std::shared_ptr<poppler::document> {
    if (!data || data->size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    return adoptRenderDocument(poppler::document::load_from_raw_data(
            data->data(), static_cast<int>(data->size()), std::string{}, password));
}

}  // namespace

using std::string;

PopplerGlibDocument::PopplerGlibDocument() = default;

PopplerGlibDocument::PopplerGlibDocument(const PopplerGlibDocument& doc):
        document(doc.document),
        renderDocumentData(doc.renderDocumentData),
        renderDocument(doc.renderDocument) {
    if (document) {
        g_object_ref(document);
    }
}

PopplerGlibDocument::~PopplerGlibDocument() {
    if (document) {
        g_object_unref(document);
        document = nullptr;
    }
}

void PopplerGlibDocument::assign(PdfDocumentInterface* doc) {
    if (document) {
        g_object_unref(document);
    }

    document = (dynamic_cast<PopplerGlibDocument*>(doc))->document;
    renderDocumentData = (dynamic_cast<PopplerGlibDocument*>(doc))->renderDocumentData;
    renderDocument = (dynamic_cast<PopplerGlibDocument*>(doc))->renderDocument;
    if (document) {
        g_object_ref(document);
    }
}

auto PopplerGlibDocument::equals(PdfDocumentInterface* doc) const -> bool {
    return document == (dynamic_cast<PopplerGlibDocument*>(doc))->document &&
           renderDocument == (dynamic_cast<PopplerGlibDocument*>(doc))->renderDocument;
}

auto PopplerGlibDocument::save(fs::path const& file, GError** error) const -> bool {
    if (document == nullptr) {
        return false;
    }

    auto uri = Util::toUri(file);
    if (!uri) {
        return false;
    }
    return poppler_document_save(document, uri->c_str(), error);
}

auto PopplerGlibDocument::load(fs::path const& file, string password, GError** error) -> bool {
    auto uri = Util::toUri(file);
    if (!uri) {
        return false;
    }

    if (document) {
        g_object_unref(document);
        document = nullptr;
    }
    renderDocument.reset();
    renderDocumentData.reset();

    this->document = poppler_document_new_from_file(uri->c_str(), password.c_str(), error);
    if (this->document) {
        this->renderDocument = makeRenderDocumentFromFile(file, password);
    }
    return this->document != nullptr;
}

auto PopplerGlibDocument::load(std::unique_ptr<std::string> data, string password, GError** error) -> bool {
    if (document) {
        g_object_unref(document);
    }
    renderDocument.reset();
    renderDocumentData = std::make_shared<std::string>(*data);

    GBytes* bytes = g_bytes_new_with_free_func(
            data->data(), data->size(), [](gpointer d) { delete reinterpret_cast<std::string*>(d); }, data.get());
    data.release();  // the string will be deleted with the bytes object
    this->document = poppler_document_new_from_bytes(bytes, password.c_str(), error);
    g_bytes_unref(bytes);  // a reference is now held by the document
    if (this->document) {
        this->renderDocument = makeRenderDocumentFromData(renderDocumentData, password);
    } else {
        renderDocumentData.reset();
    }

    return this->document != nullptr;
}

auto PopplerGlibDocument::isLoaded() const -> bool { return this->document != nullptr; }

void PopplerGlibDocument::reset() {
    if (document) {
        g_object_unref(document);
        document = nullptr;
    }
    renderDocumentData.reset();
    renderDocument.reset();
}

auto PopplerGlibDocument::getPage(size_t page) const -> PdfPagePtr {
    if (document == nullptr) {
        return nullptr;
    }

    PopplerPage* pg = poppler_document_get_page(document, int(page));
    PdfPagePtr pageptr = std::make_shared<PopplerGlibPage>(pg, document, renderDocument);
    g_object_unref(pg);

    return pageptr;
}

auto PopplerGlibDocument::getPageCount() const -> size_t {
    if (document == nullptr) {
        return 0;
    }

    return size_t(poppler_document_get_n_pages(document));
}

auto PopplerGlibDocument::getContentsIter() const -> PdfBookmarkIterator* {
    if (document == nullptr) {
        return nullptr;
    }

    PopplerIndexIter* iter = poppler_index_iter_new(document);

    if (iter == nullptr) {
        return nullptr;
    }

    return new PopplerGlibPageBookmarkIterator(iter, document);
}
