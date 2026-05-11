/*
 * VertexNote
 *
 * Paper size type and paper orientation enum
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#include "control/settings/PageTemplateSettings.h"
#include "config-features.h"

#ifdef ENABLE_LEGACY_GTK_SHELL
#include "util/raii/GtkPaperSizeUPtr.h"
#endif

#pragma once

enum class PaperOrientation : bool { HORIZONTAL = false, VERTICAL = true };

class PaperSize {
public:
    /*
     * Width of the paper stored in the standard unit "points"
     */
    double width;
    /*
     * Height of the paper stored in the standard unit "points"
     */
    double height;

    /**
     * @brief Checks that the dimensions and orientation equal (width and height cannot be swapped)
     * @param other The second PaperSize struct
     * @return Whether both PaperSize structs are exactly equal
     */
    auto operator==(const PaperSize& other) const -> bool;
    auto operator!=(const PaperSize& other) const -> bool;

    /**
     * @brief Checks that the dimensions are equal, not checking for equality of orientation (width and height can be
     * swapped)
     * @param other The second PaperSize struct
     * @return Whether both PaperSize structs have equal dimensions
     */
    auto equalDimensions(const PaperSize& other) const -> bool;

    /**
     * @brief Swapping width and height, effectively reversing orientation
     */
    void swapWidthHeight();
    [[nodiscard]] auto orientation() const -> PaperOrientation;

    // Constructors
#ifdef ENABLE_LEGACY_GTK_SHELL
    explicit PaperSize(const vn::util::GtkPaperSizeUPtr& gtkPaperSize);
#endif
    explicit PaperSize(const PageTemplateSettings& model);
    PaperSize(double width, double height);
};
