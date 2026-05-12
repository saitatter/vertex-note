/*
 * VertexNote
 *
 * PDF Page Abstraction Interface
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>  // for uint8_t
#include <memory>   // for shared_ptr
#include <string>   // for string
#include <vector>   // for vector

#include "util/NamespaceAliases.h"
#include "util/RasterImageData.h"

#include "PdfAction.h"

class PdfLink;

/// Determines how text is selected on a user action.
enum class PdfPageSelectionStyle : uint8_t {
    /// Standard selection, where all text between start and end positions is selected.
    Linear,
    /// Select a single word.
    Word,
    /// Select a single line.
    Line,
    /// Select an area.
    Area,
};

class PdfRectangle {
public:
    PdfRectangle() = default;
    PdfRectangle(double x1, double y1, double x2, double y2);

public:
    double x1 = -1;
    double y1 = -1;
    double x2 = -1;
    double y2 = -1;
};

class PdfPage {
public:
    struct TextSelection {
        std::vector<PdfRectangle> rects;
    };

    struct Link {
        PdfRectangle bounds;
        std::unique_ptr<PdfAction> action;
    };

    virtual double getWidth() const = 0;
    virtual double getHeight() const = 0;

    /// Renders the page to a detached raster buffer for previews.
    virtual vn::util::RasterImageData renderPreviewRaster(int pixelWidth, int pixelHeight, double pageWidth,
                                                          double pageHeight) const = 0;

    virtual std::vector<PdfRectangle> findText(const std::string& text) = 0;

    /// Retrieve the text contained in the provided rectangle using the given
    /// selection style.
    /// @param rect start and end points
    /// @param style The text selection style
    /// @return The selected text.
    virtual std::string selectText(const PdfRectangle& rect, PdfPageSelectionStyle style) = 0;

    /// Retrieve the set of rectangles that represent each line of text selected
    /// in the given rectangle with the given text selection style.
    /// @param rect start and end points
    /// @param style The text selection style
    /// @return The rectangles that cover the text that would be selected.
    virtual TextSelection selectTextLines(const PdfRectangle& rect, PdfPageSelectionStyle style) = 0;

    /**
     * @return A list of Links in the current page.
     */
    virtual auto getLinks() -> std::vector<Link> = 0;

    virtual int getPageId() const = 0;

    virtual std::string getPageLabel() const = 0;

private:
};

typedef std::shared_ptr<PdfPage> PdfPagePtr;
