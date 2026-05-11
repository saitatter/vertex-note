/*
 * VertexNote
 *
 * GTK color conversions.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <limits>

#include <gdk/gdk.h>

#include "util/Color.h"

namespace Util {

constexpr auto rgb_to_GdkRGBA(Color color) -> GdkRGBA;
constexpr auto argb_to_GdkRGBA(Color color) -> GdkRGBA;
constexpr auto argb_to_GdkRGBA(Color color, double alpha) -> GdkRGBA;
constexpr auto GdkRGBA_to_argb(const GdkRGBA& color) -> Color;
constexpr auto GdkRGBA_to_rgb(const GdkRGBA& color) -> Color;
constexpr auto GdkRGBA_to_ColorU16(const GdkRGBA& color) -> ColorU16;

}  // namespace Util

constexpr auto Util::rgb_to_GdkRGBA(Color color) -> GdkRGBA {
    color.alpha = 0xFF;
    return Util::argb_to_GdkRGBA(color);
}

constexpr auto Util::argb_to_GdkRGBA(const Color color) -> GdkRGBA {
    return {color.red / 255.0, color.green / 255.0, color.blue / 255.0, color.alpha / 255.0};
}

constexpr auto Util::argb_to_GdkRGBA(Color color, double alpha) -> GdkRGBA {
    return {color.red / 255.0, color.green / 255.0, color.blue / 255.0, alpha};
}

constexpr auto Util::GdkRGBA_to_argb(const GdkRGBA& color) -> Color {
    auto ret = GdkRGBA_to_rgb(color);
    ret.alpha = floatToUIntColor(color.alpha);
    return ret;
}

constexpr auto Util::GdkRGBA_to_rgb(const GdkRGBA& color) -> Color {
    return Color{floatToUIntColor(color.red), floatToUIntColor(color.green), floatToUIntColor(color.blue)};
}

constexpr auto Util::GdkRGBA_to_ColorU16(const GdkRGBA& color) -> ColorU16 {
    auto floatToColorU16 = [](double component) {
        constexpr double MAX_COLOR = 65536.0 - std::numeric_limits<double>::epsilon() * (65536.0 / 2.0);
        static_assert(MAX_COLOR < 65536.0, "MAX_COLOR isn't smaller than 65536");
        return static_cast<uint16_t>(component * MAX_COLOR);
    };

    return {floatToColorU16(color.red), floatToColorU16(color.green), floatToColorU16(color.blue),
            floatToColorU16(color.alpha)};
}
