/*
 * VertexNote
 *
 * Pure page layout helpers for the Qt canvas.
 */

#include "QtCanvasLayout.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr double PAGE_STACK_X = 92.0;
constexpr double PAGE_STACK_Y = 84.0;
constexpr double PAGE_STACK_GAP = 48.0;

}  // namespace

auto layoutQtCanvasPages(const std::vector<vn::view::render::PageRenderSnapshot>& pages,
                         QtCanvasLayoutOptions options) -> std::vector<QRectF> {
    std::vector<QRectF> rects;
    if (pages.empty()) {
        rects.emplace_back(PAGE_STACK_X, PAGE_STACK_Y, 1100.0, 1500.0);
        return rects;
    }

    rects.reserve(pages.size());

    const int spanValue = options.span == 0 ? 1 : options.span;
    const std::size_t spanCount = static_cast<std::size_t>(std::clamp(std::abs(spanValue), 1, 8));
    const bool fixedRows = spanValue < 0;
    std::vector<std::pair<std::size_t, std::size_t>> cells;
    std::vector<double> columnWidths;
    std::vector<double> rowHeights;
    cells.reserve(pages.size());
    for (std::size_t index = 0; index < pages.size(); ++index) {
        const auto& page = pages[index];
        const double pageWidth = std::max(page.width, 1.0);
        const double pageHeight = std::max(page.height, 1.0);
        std::size_t row = 0U;
        std::size_t column = 0U;
        if (fixedRows) {
            row = index % spanCount;
            column = index / spanCount;
        } else if (options.vertical) {
            column = index % spanCount;
            row = index / spanCount;
        } else {
            row = index % spanCount;
            column = index / spanCount;
        }

        if (column >= columnWidths.size()) {
            columnWidths.resize(column + 1U, 0.0);
        }
        if (row >= rowHeights.size()) {
            rowHeights.resize(row + 1U, 0.0);
        }
        columnWidths[column] = std::max(columnWidths[column], pageWidth);
        rowHeights[row] = std::max(rowHeights[row], pageHeight);
        cells.emplace_back(row, column);
    }

    std::vector<double> columnOffsets(columnWidths.size(), PAGE_STACK_X);
    for (std::size_t index = 1; index < columnOffsets.size(); ++index) {
        columnOffsets[index] = columnOffsets[index - 1U] + columnWidths[index - 1U] + PAGE_STACK_GAP;
    }
    std::vector<double> rowOffsets(rowHeights.size(), PAGE_STACK_Y);
    for (std::size_t index = 1; index < rowOffsets.size(); ++index) {
        rowOffsets[index] = rowOffsets[index - 1U] + rowHeights[index - 1U] + PAGE_STACK_GAP;
    }

    for (std::size_t index = 0; index < pages.size(); ++index) {
        const auto& page = pages[index];
        const double pageWidth = std::max(page.width, 1.0);
        const double pageHeight = std::max(page.height, 1.0);
        const auto [row, column] = cells[index];

        double x = PAGE_STACK_X;
        double y = PAGE_STACK_Y;
        if (options.vertical) {
            x = columnOffsets[column];
            y = rowOffsets[row];
        } else {
            x = rowOffsets[row];
            y = columnOffsets[column];
        }

        rects.emplace_back(x, y, pageWidth, pageHeight);
    }

    if (options.rightToLeft || options.bottomToTop) {
        QRectF bounds = rects.front();
        for (std::size_t index = 1; index < rects.size(); ++index) {
            bounds = bounds.united(rects[index]);
        }
        for (auto& rect: rects) {
            if (options.rightToLeft) {
                rect.moveLeft(bounds.right() - (rect.left() - bounds.left()) - rect.width());
            }
            if (options.bottomToTop) {
                rect.moveTop(bounds.bottom() - (rect.top() - bounds.top()) - rect.height());
            }
        }
    }
    return rects;
}

