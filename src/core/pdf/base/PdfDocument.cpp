#include "PdfDocument.h"

#include "pdf/base/PdfDocumentInterface.h"    // for PdfDocumentInterface
#include "pdf/base/PdfPage.h"                 // for PdfPagePtr
#include "pdf/popplerapi/PopplerPdfDocument.h"  // for PopplerPdfDocument

#include "filesystem.h"  // for path

class PdfBookmarkIterator;

PdfDocument::PdfDocument(): doc(new PopplerPdfDocument()) {}

PdfDocument::PdfDocument(const PdfDocument& doc): doc(new PopplerPdfDocument()) {
    this->doc->assign(doc.doc);
}

PdfDocument::~PdfDocument() {
    delete doc;
    doc = nullptr;
}

auto PdfDocument::operator=(const PdfDocument& doc) -> PdfDocument& {
    this->doc->assign(doc.doc);
    return *this;
}

auto PdfDocument::operator==(PdfDocument& doc) const -> bool { return this->doc->equals(doc.doc); }

void PdfDocument::assign(PdfDocumentInterface* doc) { this->doc->assign(doc); }

auto PdfDocument::equals(PdfDocumentInterface* doc) const -> bool { return this->doc->equals(doc); }

auto PdfDocument::save(fs::path const& file, std::string* errorMessage) const -> bool {
    return doc->save(file, errorMessage);
}

auto PdfDocument::load(fs::path const& file, std::string password, std::string* errorMessage) -> bool {
    return doc->load(file, password, errorMessage);
}

auto PdfDocument::load(std::unique_ptr<std::string> data, std::string password, std::string* errorMessage) -> bool {
    return doc->load(std::move(data), password, errorMessage);
}

auto PdfDocument::isLoaded() const -> bool { return doc->isLoaded(); }

void PdfDocument::reset() { doc->reset(); }

auto PdfDocument::getPage(size_t page) const -> PdfPagePtr { return doc->getPage(page); }

auto PdfDocument::getPageCount() const -> size_t { return doc->getPageCount(); }

auto PdfDocument::getContentsIter() const -> PdfBookmarkIterator* { return doc->getContentsIter(); }
