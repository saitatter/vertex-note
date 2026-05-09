/*
 * VertexNote
 *
 * Class for backgrounds with lineWidth and a line color
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string

#include "util/Color.h"  // for Color

#include "PlainBackgroundView.h"  // for PlainBackgroundView

class BackgroundConfig;

namespace vn::view {
class OneColorBackgroundView: public PlainBackgroundView {
public:
    OneColorBackgroundView(double pageWidth, double pageHeight, Color backgroundColor, const BackgroundConfig& config,
                           double defaultLineWidth, Color defaultLineColor, Color altDefaultLineColor);
    virtual ~OneColorBackgroundView() = default;

    void multiplyLineWidth(double factor);

protected:
    /**
     * @brief Get the config's Hex value associated to the config string and make it a color.
     * Fallback to defaultColor if the value does not exist in config.
     */
    static Color getColorOr(const BackgroundConfig& config, const std::string& str, const Color& defaultColor);

protected:
    Color foregroundColor;
    double lineWidth;
};
};  // namespace vn::view
