#include "PopplerGlibDocument.h"

#include <limits>
#include <memory>    // for make_shared, unique_ptr

#include <poppler-document.h>  // for poppler_document_get_page
#include <poppler/cpp/poppler-document.h>

#include "util/PathUtil.h"  // for toUri
#include "util/StringUtils.h"

#include "PopplerGlibPage.h"                  // for PopplerGlibPage
#include "PopplerGlibPageBookmarkIterator.h"  // for PopplerGlibPageBookmark...
#include "filesystem.h"                       // for path

class PdfBookmarkIterator;

namespace {

auto adoptDocument(poppler::document* document) -> std::shared_ptr<poppler::document> {
    return std::shared_ptr<poppler::document>(document, [](poppler::document* ptr) { delete ptr; });
}

auto makeDocumentFromFile(const fs::path& file, const std::string& password) -> std::shared_ptr<poppler::document> {
    const auto fileName = file.generic_u8string();
    return adoptDocument(poppler::document::load_from_file(std::string{char_cast(fileName)}, std::string{}, password));
}

auto makeDocumentFromData(const std::shared_ptr<std::string>& data, const std::string& password)
        -> std::shared_ptr<poppler::document> {
    if (!data || data->size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    return adoptDocument(poppler::document::load_from_raw_data(data->data(), static_cast<int>(data->size()),
                                                               std::string{}, password));
}

}  // namespace

using std::string;

PopplerGlibDocument::PopplerGlibDocument() = default;

PopplerGlibDocument::PopplerGlibDocument(const PopplerGlibDocument& doc):
        linkDocument(doc.linkDocument),
        documentData(doc.documentData),
        document(doc.document) {
    if (linkDocument) {
        g_object_ref(linkDocument);
    }
}

PopplerGlibDocument::~PopplerGlibDocument() {
    if (linkDocument) {
        g_object_unref(linkDocument);
        linkDocument = nullptr;
    }
}

void PopplerGlibDocument::assign(PdfDocumentInterface* doc) {
    if (linkDocument) {
        g_object_unref(linkDocument);
    }

    const auto* popplerDoc = dynamic_cast<PopplerGlibDocument*>(doc);
    linkDocument = popplerDoc->linkDocument;
    documentData = popplerDoc->documentData;
    document = popplerDoc->document;
    if (linkDocument) {
        g_object_ref(linkDocument);
    }
}

auto PopplerGlibDocument::equals(PdfDocumentInterface* doc) const -> bool {
    return document == (dynamic_cast<PopplerGlibDocument*>(doc))->document &&
           linkDocument == (dynamic_cast<PopplerGlibDocument*>(doc))->linkDocument;
}

auto PopplerGlibDocument::save(fs::path const& file, GError** error) const -> bool {
    if (document == nullptr) {
        return false;
    }

    (void) error;
    const auto fileName = file.generic_u8string();
    return document->save(std::string{char_cast(fileName)});
}

auto PopplerGlibDocument::load(fs::path const& file, string password, GError** error) -> bool {
    if (linkDocument) {
        g_object_unref(linkDocument);
        linkDocument = nullptr;
    }
    document.reset();
    documentData.reset();

    document = makeDocumentFromFile(file, password);
    if (!document) {
        (void) error;
        return false;
    }

    auto uri = Util::toUri(file);
    if (uri) {
        GError* sidecarError = nullptr;
        linkDocument = poppler_document_new_from_file(uri->c_str(), password.c_str(), &sidecarError);
        if (sidecarError) {
            g_error_free(sidecarError);
        }
    }

    return true;
}

auto PopplerGlibDocument::load(std::unique_ptr<std::string> data, string password, GError** error) -> bool {
    if (linkDocument) {
        g_object_unref(linkDocument);
        linkDocument = nullptr;
    }
    document.reset();
    documentData = std::make_shared<std::string>(*data);

    document = makeDocumentFromData(documentData, password);
    if (!document) {
        documentData.reset();
        (void) error;
        return false;
    }

    GBytes* bytes = g_bytes_new_static(documentData->data(), documentData->size());
    GError* sidecarError = nullptr;
    linkDocument = poppler_document_new_from_bytes(bytes, password.c_str(), &sidecarError);
    g_bytes_unref(bytes);
    if (sidecarError) {
        g_error_free(sidecarError);
    }

    return true;
}

auto PopplerGlibDocument::isLoaded() const -> bool { return this->document != nullptr; }

void PopplerGlibDocument::reset() {
    if (linkDocument) {
        g_object_unref(linkDocument);
        linkDocument = nullptr;
    }
    documentData.reset();
    document.reset();
}

auto PopplerGlibDocument::getPage(size_t page) const -> PdfPagePtr {
    if (document == nullptr || page >= static_cast<std::size_t>(document->pages())) {
        return nullptr;
    }

    PopplerPage* linkPage = linkDocument ? poppler_document_get_page(linkDocument, int(page)) : nullptr;
    PdfPagePtr pageptr =
            std::make_shared<PopplerGlibPage>(static_cast<int>(page), document, linkPage, linkDocument);
    if (linkPage) {
        g_object_unref(linkPage);
    }

    return pageptr;
}

auto PopplerGlibDocument::getPageCount() const -> size_t {
    if (document == nullptr) {
        return 0;
    }

    return size_t(document->pages());
}

auto PopplerGlibDocument::getContentsIter() const -> PdfBookmarkIterator* {
    if (linkDocument == nullptr) {
        return nullptr;
    }

    PopplerIndexIter* iter = poppler_index_iter_new(linkDocument);

    if (iter == nullptr) {
        return nullptr;
    }

    return new PopplerGlibPageBookmarkIterator(iter, linkDocument);
}
