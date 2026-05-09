#include "PdfElemSelection.h"

#include <algorithm>  // for max, min
#include <limits>   // for numeric_limits
#include <memory>   // for __shared_ptr_access
#include <utility>  // for move

#include <cairo.h>    // for cairo_line_to, cairo_region_destroy
#include <gdk/gdk.h>  // for GdkRGBA, gdk_cairo_set_source_rgba

#include "control/Control.h"      // for Control
#include "control/ToolHandler.h"  // for ToolHandler
#include "gui/PageView.h"         // for PageView
#include "gui/VertexNoteView.h"      // for VertexNoteView
#include "model/Document.h"       // for Document
#include "model/PageRef.h"        // for PageRef
#include "model/NotePage.h"        // for NotePage
#include "pdf/base/PdfPage.h"  // for PdfRectangle, PdfPageSelectio...
#include "util/Assert.h"          // for xoj_assert
#include "util/safe_casts.h"      // for strict_cast, as_signed, as_si...
#include "view/overlays/PdfElementSelectionView.h"

PdfElemSelection::PdfElemSelection(double x, double y, Control* control):
        pdf(nullptr),
        bounds({x, y, x, y}),
        finalized(false),
        viewPool(std::make_shared<vn::util::DispatchPool<vn::view::PdfElementSelectionView>>()) {

    if (auto pNr = control->getCurrentPage()->getPdfPageNr(); pNr != npos) {
        Document* doc = control->getDocument();
        doc->lock_shared();
        this->pdf = doc->getPdfPage(pNr);
        doc->unlock_shared();

        this->selectionPageNr = pNr;
    }

    this->toolType = control->getToolHandler()->getToolType();
}

PdfElemSelection::~PdfElemSelection() {
    Range rg = getRegionBbox();
    this->viewPool->dispatchAndClear(vn::view::PdfElementSelectionView::CANCEL_SELECTION_REQUEST, rg);
}

auto PdfElemSelection::finalizeSelectionAndRepaint(PdfPageSelectionStyle style) -> bool {
    Range rg = getRegionBbox();
    bool result = this->finalizeSelection(style);
    rg = rg.unite(getRegionBbox());
    if (!rg.empty()) {
        this->viewPool->dispatch(vn::view::PdfElementSelectionView::FLAG_DIRTY_REGION_REQUEST, rg);
    }
    return result;
}

bool PdfElemSelection::finalizeSelection(PdfPageSelectionStyle style) {
    this->finalized = true;

    PdfPage::TextSelection selection = this->pdf->selectTextLines(this->bounds, style);
    this->selectedTextRegion = std::move(selection.region);
    this->selectedTextRects = std::move(selection.rects);
    this->selectedText = this->pdf->selectText(this->bounds, style);
    // Informs the windowing system of the selection -- i.e. for accessibility purposes
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), this->selectedText.c_str(),
                           strict_cast<gint>(this->selectedText.length()));
    return !this->selectedTextRects.empty();
}

PdfPageSelectionStyle PdfElemSelection::selectionStyleForToolType(ToolType type) {
    switch (type) {
        case ToolType::TOOL_SELECT_PDF_TEXT_RECT:
            return PdfPageSelectionStyle::Area;
        default:
            return PdfPageSelectionStyle::Linear;
    }
}

Range PdfElemSelection::getRegionBbox() const {
    if (this->selectedTextRegion && cairo_region_num_rectangles(this->selectedTextRegion.get()) > 0) {
        cairo_rectangle_int_t bbox{};
        cairo_region_get_extents(this->selectedTextRegion.get(), &bbox);
        return Range(bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height);
    }
    return Range();  // empty range
}

void PdfElemSelection::currentPos(double x, double y, PdfPageSelectionStyle style) {
    if (!this->pdf) {
        return;
    }

    // Update the end position
    this->bounds.x2 = x;
    this->bounds.y2 = y;
    Range rg = getRegionBbox();

    // Repaint the selected text area
    switch (style) {
        case PdfPageSelectionStyle::Linear:
        case PdfPageSelectionStyle::Word:
        case PdfPageSelectionStyle::Line:
            this->selectedTextRegion.reset(this->pdf->selectTextRegion(this->bounds, style), vn::util::adopt);
            break;
        case PdfPageSelectionStyle::Area: {
            cairo_rectangle_int_t rect;
            rect.x = floor_cast<int>(std::min(bounds.x1, bounds.x2));
            rect.width = ceil_cast<int>(std::max(bounds.x1, bounds.x2)) - rect.x;
            rect.y = floor_cast<int>(std::min(bounds.y1, bounds.y2));
            rect.height = ceil_cast<int>(std::max(bounds.y1, bounds.y2)) - rect.y;
            this->selectedTextRegion.reset(cairo_region_create_rectangle(&rect), vn::util::adopt);
        } break;
        default:
            xoj_assert_message(false, "Unreachable");
    }
    xoj_assert(this->selectedTextRegion);

    rg = rg.unite(getRegionBbox());
    if (!rg.empty()) {
        this->viewPool->dispatch(vn::view::PdfElementSelectionView::FLAG_DIRTY_REGION_REQUEST, rg);
    }
}

auto PdfElemSelection::contains(double x, double y) -> bool {
    if (!this->selectedTextRegion) {
        return false;
    }

    return cairo_region_contains_point(this->selectedTextRegion.get(), static_cast<int>(x), static_cast<int>(y));
}

auto PdfElemSelection::getSelectedTextRects() const -> const std::vector<PdfRectangle>& { return selectedTextRects; }

auto PdfElemSelection::getSelectedText() const -> const std::string& { return this->selectedText; }

auto PdfElemSelection::getSelectionPageNr() const -> size_t { return selectionPageNr; }

auto PdfElemSelection::isFinalized() const -> bool { return this->finalized; }

void PdfElemSelection::setToolType(ToolType tType) { this->toolType = tType; }
