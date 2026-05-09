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

#include <string>

#include "control/jobs/BaseExportJob.h"
#include "util/ElementRange.h"

#include "filesystem.h"

class PdfExport {
public:
    PdfExport();
    virtual ~PdfExport();

public:
    virtual bool createPdf(fs::path const& file, bool progressiveMode) = 0;
    virtual bool createPdf(fs::path const& file, const PageRangeVector& range, bool progressiveMode) = 0;
    virtual std::string getLastError() = 0;

    /**
     * Export without background
     */
    virtual void setExportBackground(ExportBackgroundType exportBackground);

    /**
     * @brief Select layers to export by parsing str
     * @param rangeStr A string parsed to get a list of layers
     */
    virtual void setLayerRange(const char* rangeStr) = 0;

private:
};
