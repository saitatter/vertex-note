/*
 * VertexNote
 *
 * Handles the layout of the pages within a Xournal document
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>     // for size_t
#include <functional>  // for function
#include <mutex>       // for mutex
#include <optional>    // for optional
#include <vector>      // for vector

#include <gtk/gtk.h>  // for GtkAdjustment

#include "LayoutMapper.h"  // for LayoutMapper

class PageView;
class VertexNoteView;
class ScrollHandling;
class Range;

namespace xoj::util {
template <typename T>
struct Point;
template <typename T>
class Rectangle;
};  // namespace vn::util

namespace vn {
namespace util = xoj::util;
}

/**
 * @brief The Layout manager for the VertexNoteWidget
 *
 * This class manages the layout of the PageView's contained
 * in the VertexNoteWidget
 */
class Layout final {
public:
    Layout(VertexNoteView* view, ScrollHandling* scrollHandling);

public:
    // Todo(Fabian): move to ScrollHandling also it must not depend on Layout
    /**
     * Increases the adjustments by the given amounts
     */
    void scrollRelative(double x, double y);

    // Todo(Fabian): move to ScrollHandling also it must not depend on Layout
    /**
     * Changes the adjustments by absolute amounts (for pinch-to-zoom)
     */
    void scrollAbs(double x, double y);

    // Todo(Fabian): move to VertexNoteView
    /**
     * Changes the adjustments in such a way as to make sure that
     * the given Rectangle is visible
     *
     * @remark If the given Rectangle won't fit into the scrolled window
     *         then only its top left corner will be visible
     */
    void ensureRectIsVisible(int x, int y, int width, int height);

    /// Returns the height of the entire Layout - including centering padding
    int getTotalPixelHeight() const;

    /// Returns the height of the entire Layout - excluding centering padding
    int getMinimalPixelHeight() const;

    /// Returns the width of the entire Layout - including centering padding
    int getTotalPixelWidth() const;

    /// Returns the width of the entire Layout - excluding centering padding
    int getMinimalPixelWidth() const;

    // Todo(Fabian): move to VertexNoteView this must not depend on Layout directly
    /**
     * Returns the Rectangle which is currently visible - in pixel coordinates
     */
    vn::util::Rectangle<double> getVisibleRect();


    /// recalculate and resize Layout
    void recalculate();

    /**
     * Recompute the centering paddings (to center the content if the allocation is too big)
     * @params the size of the GtkAllocation of the GtkVertexNote instance - or -1 for computation from the GtkAdjustments
     */
    void recomputeCenteringPadding(int allocWidth = -1, int allocHeight = -1);

    // Todo(Fabian): move to View:
    /**
     * Updates the current PageView. The PageView is selected based on
     * the percentage of the visible area of the PageView relative
     * to its total area.
     */
    void updateVisibility();

    /**
     * Return the pageview containing coordinates (in pixel coordinates)
     */
    PageView* getPageViewAt(int x, int y) const;

    /**
     * Return the page index found (or std::nullopt if not found) when moving by the given offsets from the ref page
     * Assumes refPageNumber is a valid page index.
     */
    std::optional<size_t> getPageWithRelativePosition(size_t refPageNumber, int columnOffset, int rowOffset) const;

    /**
     * Get the fixed padding (in pixels) on the left and above the given point.
     * The return values do not take the centering padding into account
     *
     * @param ref The reference point, in pixel coordinates
     */
    vn::util::Point<int> getFixedPaddingBeforePoint(const vn::util::Point<double>& ref) const;

    /// Get the zoom-dependent padding, added to center the page when zoomed out
    vn::util::Point<int> getCenteringPadding() const;

    /// Returns a list of the indices of the visible pages
    std::vector<size_t> getVisiblePages() const;

    vn::util::Point<int> getPixelCoordinatesOfEntry(vn::util::Point<int> gridCoords) const;
    vn::util::Point<int> getPixelCoordinatesOfEntry(size_t n) const;

    /**
     * Execute the given function for each entry that intersects the range. entryIndex is the entry, intersection is the
     * intersection (in pixel coordinates) and pixelPosition is the entry's upper left corner in pixel coordinates
     */
    void forEachEntriesIntersectingRange(
            const Range& rg,
            std::function<void(size_t entryIndex, const Range& intersection, vn::util::Point<int> pixelPosition)> fun)
            const;

protected:
    /// Same as above but does not lock the mutex
    vn::util::Point<int> getPixelCoordinatesOfEntryUnsafe(vn::util::Point<int> gridCoords) const;
    /// Same as above but does not lock the mutex
    vn::util::Point<int> getPixelCoordinatesOfEntryUnsafe(size_t n) const;

    /// Same as above but does not lock the mutex
    int getTotalPixelWidthUnsafe() const;
    /// Same as above but does not lock the mutex
    int getMinimalPixelWidthUnsafe() const;
    /// Same as above but does not lock the mutex
    int getTotalPixelHeightUnsafe() const;
    /// Same as above but does not lock the mutex
    int getMinimalPixelHeightUnsafe() const;
    /// Same as above but does not lock the mutex
    void recomputeCenteringPaddingUnsafe(int allocWidth, int allocHeight);

    /// Convert pixel-coordinates to the grid position containing them
    GridPosition getGridPositionAtUnsafe(const vn::util::Point<double>& p) const;

    // Todo(Fabian): move to ScrollHandling also it must not depend on Layout
    static void horizontalScrollChanged(GtkAdjustment* adjustment, Layout* layout);
    static void verticalScrollChanged(GtkAdjustment* adjustment, Layout* layout);

private:
    void computePrecalculated();

    void maybeAddLastPage(Layout* layout);

public:
    struct PreCalculated {
        mutable std::mutex m;

        LayoutMapper mapper;

        std::vector<double> widthCols;   ///< In page coordinates - multiply by zoom to get pixels
        std::vector<double> heightRows;  ///< In page coordinates - multiply by zoom to get pixels

        std::vector<double> stretchableHorizontalPixelsAfterColumn;  ///< Stretchable - multiply by zoom to get pixels
        std::vector<double> stretchableVerticalPixelsAfterRow;       ///< Stretchable - multiply by zoom to get pixels
        int paddingLeft;                                             ///< in pixels
        int paddingRight;                                            ///< in pixels
        int paddingTop;                                              ///< in pixels
        int paddingBottom;                                           ///< in pixels

        int horizontalCenteringPadding;  ///< Added before and after if the allocation is too big
        int verticalCenteringPadding;    ///< Added before and after if the allocation is too big
    };

private:
    struct PixelCounter;  ///< Used to get the pixel coordinates of entries

    VertexNoteView* view = nullptr;
    ScrollHandling* scrollHandling = nullptr;

    // Todo(Fabian): move to ScrollHandling also it must not depend on Layout
    double lastScrollHorizontal = -1;
    double lastScrollVertical = -1;

    std::vector<size_t> previouslyVisiblePages;  ///< indexes of pages with PageView::isVisible() == true

    PreCalculated pc{};

    /// Used to have only one call when zooming in/out
    bool blockHorizontalCallback = false;
};
