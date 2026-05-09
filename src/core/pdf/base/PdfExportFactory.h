/*
 * VertexNote
 *
 * PDF Document Export Abstraction Interface
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for unique_ptr

#include "PdfExportBackend.h"

class Document;
class ProgressListener;
class PdfExport;

class PdfExportFactory {
private:
    PdfExportFactory();
    ~PdfExportFactory();

public:
    static std::unique_ptr<PdfExport> createExport(const Document* doc, ProgressListener* listener,
                                                      ExportBackend backend = ExportBackend::DEFAULT);
};
