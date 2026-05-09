#include "PdfExportFactory.h"

#include "model/Document.h"

#include "QPdfExport.h"
#include "CairoPdfExport.h"  // for CairoPdfExport

class PdfExport;

PdfExportFactory::PdfExportFactory() = default;

PdfExportFactory::~PdfExportFactory() = default;

auto PdfExportFactory::createExport(const Document* doc, ProgressListener* listener, ExportBackend backend)
        -> std::unique_ptr<PdfExport> {
    if (!doc->getPdfFilepath().empty()) {
        switch (backend) {
            case ExportBackend::DEFAULT:  // fallback to qpdf/podofo/mupdf/cairo in that order
#ifdef ENABLE_QPDF
            case ExportBackend::QPDF:
                return std::make_unique<QPdfExport>(doc, listener);
#endif
            case ExportBackend::CAIRO:
            default:  // The requested backend has not been included in this build
                return std::make_unique<CairoPdfExport>(doc, listener);
        }
    }
    return std::make_unique<CairoPdfExport>(doc, listener);
}
