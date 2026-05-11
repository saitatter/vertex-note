/*
 * VertexNote
 *
 * Color utility, does color conversions
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once
#include "util/Color.h"


class Recolor {

public:
    Recolor(const ColorU8& light, const ColorU8& dark);
    Recolor() = default;

public:
    const ColorU8& getDark() const;
    const ColorU8& getLight() const;

    ColorU8 convertColor(const ColorU8& other) const;

private:
    constexpr friend bool operator==(Recolor const& lhs, Recolor const& rhs) {
        return lhs.difference == rhs.difference && lhs.offset == rhs.offset && lhs.ref == rhs.ref;
    }

    void recalcDiffAndOff();

private:
    // parameters set by the user also needed to save the settings to file
    ColorU8 dark = {};
    ColorU8 light = {};

    // calculated from above parameters to avoid having to calculate them for every recoloring all over again
    ColorU8 difference = {};  // abs(dark - light)
    ColorU8 offset = {};      // min(dark, light)
    ColorU8 ref = {};         // for recoloring light
};
