#include "PopplerPdfDocument.h"

#include <limits>
#include <memory>    // for make_shared, unique_ptr
#include <optional>
#include <utility>

#include <Object.h>
#include <Outline.h>
#include <PDFDoc.h>
#include <Stream.h>
#include <goo/GooString.h>
#include <poppler/cpp/poppler-document.h>

#include "util/StringUtils.h"

#include "PopplerPdfPage.h"                  // for PopplerPdfPage
#include "PopplerPdfPageBookmarkIterator.h"  // for PopplerPdfPageBookmark...
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

auto makePassword(const std::string& password) -> std::optional<GooString> {
    if (password.empty()) {
        return std::nullopt;
    }
    return GooString(password);
}

auto makeLinkDocumentFromFile(const fs::path& file, const std::string& password) -> std::shared_ptr<PDFDoc> {
    const auto fileName = file.generic_u8string();
    auto doc = std::make_shared<PDFDoc>(std::make_unique<GooString>(std::string{char_cast(fileName)}),
                                        std::optional<GooString>{}, makePassword(password));
    return doc->isOk() ? doc : nullptr;
}

auto makeLinkDocumentFromData(const std::shared_ptr<std::string>& data, const std::string& password)
        -> std::shared_ptr<PDFDoc> {
    if (!data) {
        return nullptr;
    }
    auto stream = std::make_unique<MemStream>(data->data(), 0, static_cast<Goffset>(data->size()), Object::null());
    auto doc = std::make_shared<PDFDoc>(std::move(stream), std::optional<GooString>{}, makePassword(password));
    return doc->isOk() ? doc : nullptr;
}

void setErrorMessage(std::string* errorMessage, std::string message) {
    if (errorMessage) {
        *errorMessage = std::move(message);
    }
}

}  // namespace

using std::string;

PopplerPdfDocument::PopplerPdfDocument() = default;

PopplerPdfDocument::PopplerPdfDocument(const PopplerPdfDocument& doc):
        linkDocument(doc.linkDocument),
        documentData(doc.documentData),
        document(doc.document) {}

PopplerPdfDocument::~PopplerPdfDocument() = default;

void PopplerPdfDocument::assign(PdfDocumentInterface* doc) {
    const auto* popplerDoc = dynamic_cast<PopplerPdfDocument*>(doc);
    linkDocument = popplerDoc->linkDocument;
    documentData = popplerDoc->documentData;
    document = popplerDoc->document;
}

auto PopplerPdfDocument::equals(PdfDocumentInterface* doc) const -> bool {
    return document == (dynamic_cast<PopplerPdfDocument*>(doc))->document &&
           linkDocument == (dynamic_cast<PopplerPdfDocument*>(doc))->linkDocument;
}

auto PopplerPdfDocument::save(fs::path const& file, std::string* errorMessage) const -> bool {
    if (document == nullptr) {
        setErrorMessage(errorMessage, "Document not loaded.");
        return false;
    }

    const auto fileName = file.generic_u8string();
    if (!document->save(std::string{char_cast(fileName)})) {
        setErrorMessage(errorMessage, "Could not save PDF document.");
        return false;
    }
    return true;
}

auto PopplerPdfDocument::load(fs::path const& file, string password, std::string* errorMessage) -> bool {
    linkDocument.reset();
    document.reset();
    documentData.reset();

    document = makeDocumentFromFile(file, password);
    if (!document) {
        setErrorMessage(errorMessage, "Could not load PDF document.");
        return false;
    }

    linkDocument = makeLinkDocumentFromFile(file, password);
    return true;
}

auto PopplerPdfDocument::load(std::unique_ptr<std::string> data, string password, std::string* errorMessage) -> bool {
    linkDocument.reset();
    document.reset();
    documentData = std::make_shared<std::string>(*data);

    document = makeDocumentFromData(documentData, password);
    if (!document) {
        documentData.reset();
        setErrorMessage(errorMessage, "Could not load PDF document.");
        return false;
    }

    linkDocument = makeLinkDocumentFromData(documentData, password);
    return true;
}

auto PopplerPdfDocument::isLoaded() const -> bool { return this->document != nullptr; }

void PopplerPdfDocument::reset() {
    linkDocument.reset();
    documentData.reset();
    document.reset();
}

auto PopplerPdfDocument::getPage(size_t page) const -> PdfPagePtr {
    if (document == nullptr || page >= static_cast<std::size_t>(document->pages())) {
        return nullptr;
    }

    PdfPagePtr pageptr =
            std::make_shared<PopplerPdfPage>(static_cast<int>(page), document, linkDocument);

    return pageptr;
}

auto PopplerPdfDocument::getPageCount() const -> size_t {
    if (document == nullptr) {
        return 0;
    }

    return size_t(document->pages());
}

auto PopplerPdfDocument::getContentsIter() const -> PdfBookmarkIterator* {
    if (linkDocument == nullptr) {
        return nullptr;
    }

    auto* outline = linkDocument->getOutline();
    if (!outline) {
        return nullptr;
    }

    const auto* items = outline->getItems();
    if (!items || items->empty()) {
        return nullptr;
    }

    return new PopplerPdfPageBookmarkIterator(items, linkDocument);
}
