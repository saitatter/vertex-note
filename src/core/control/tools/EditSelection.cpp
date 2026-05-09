#include "EditSelection.h"

#include <algorithm>  // for min, max, stable_sort
#include <cmath>      // for abs, cos, sin, cop...
#include <cstddef>    // for size_t
#include <limits>     // for numeric_limits
#include <memory>     // for make_unique, __sha...
#include <numeric>    // for reduce
#include <optional>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>     // for string
#include <utility>

#include <gdk/gdk.h>  // for gdk_cairo_set_sour...

#include "control/Control.h"                       // for Control
#include "control/settings/Settings.h"             // for Settings
#include "control/tools/CursorSelectionType.h"     // for CURSOR_SELECTION_NONE
#include "control/tools/SnapToGridInputHandler.h"  // for SnapToGridInputHan...
#include "control/zoom/ZoomControl.h"              // for ZoomControl
#include "gui/Layout.h"                            // for Layout
#include "gui/PageView.h"                          // for PageView
#include "gui/VertexNoteView.h"                       // for VertexNoteView
#include "gui/VertexNoteCursor.h"                   // for VertexNoteCursor
#include "model/Document.h"                        // for Document
#include "model/Element.h"                         // for Element::Index
#include "model/ElementInsertionPosition.h"
#include "model/Layer.h"                          // for Layer
#include "model/LineStyle.h"                      // for LineStyle
#include "model/Point.h"                          // for Point
#include "model/NotePage.h"                        // for NotePage
#include "undo/ArrangeUndoAction.h"               // for ArrangeUndoAction
#include "undo/GeometryTopologyUndoAction.h"
#include "undo/GeometryVertexMoveUndoAction.h"
#include "undo/InsertUndoAction.h"                // for InsertsUndoAction
#include "undo/UndoRedoHandler.h"                 // for UndoRedoHandler
#include "util/Range.h"                           // for Range
#include "util/Util.h"                            // for cairo_set_dash_from_vector
#include "util/glib_casts.h"                      // for wrap_v
#include "util/i18n.h"                            // for _
#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream
#include "view/overlays/SnapIndicatorViewHelper.h"
#include "vertexnote/constraints/GeometryConstraintSolver.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/PageGeometryCollector.h"

#include "EditSelectionContents.h"  // for EditSelectionConte...

class NoteFont;

using std::vector;
using vn::util::Rectangle;

/// Smallest can scale down to, in pixels.
constexpr size_t MINPIXSIZE = 5;

/// Padding for ui buttons
constexpr int DELETE_PADDING = 20;
constexpr int ROTATE_PADDING = 8;
constexpr double SELECTION_PADDING = 12.;

/// Number of times to trigger edge pan timer per second
constexpr unsigned int PAN_TIMER_RATE = 30;
constexpr double GEOMETRY_SNAP_RADIUS_PIXELS = 8.0;

namespace SelectionFactory {
/// @return Bounds and SnappingBounds
static auto computeBoxes(const InsertionOrder& elts) -> std::pair<Range, Range> {
    return std::transform_reduce(
            elts.begin(), elts.end(), std::pair<Range, Range>(),
            [](auto&& p, auto&& q) {
                return std::pair<Range, Range>(p.first.unite(q.first), p.second.unite(q.second));
            },
            [](auto&& e) { return std::make_pair(Range(e.e->boundingRect()), Range(e.e->getSnappedBounds())); });
}

static auto distanceToSegment(double px, double py, double ax, double ay, double bx, double by)
        -> std::pair<double, double> {
    const double dx = bx - ax;
    const double dy = by - ay;
    const double lengthSquared = dx * dx + dy * dy;

    if (lengthSquared == 0.0) {
        return {std::hypot(px - ax, py - ay), 0.0};
    }

    const double t = std::clamp(((px - ax) * dx + (py - ay) * dy) / lengthSquared, 0.0, 1.0);
    const double projX = ax + t * dx;
    const double projY = ay + t * dy;
    return {std::hypot(px - projX, py - projY), t};
}

static auto edgeLength(const vn::geom::GeometryObject& object, const vn::geom::Constraint& constraint) -> double {
    if (constraint.vertices.size() < 2U) {
        return 0.0;
    }

    const auto* start = object.vertex(constraint.vertices.front());
    const auto* end = object.vertex(constraint.vertices[1]);
    if (!start || !end) {
        return 0.0;
    }

    return std::hypot(end->position.x - start->position.x, end->position.y - start->position.y);
}

static auto intersectsSelection(std::span<const vn::geom::VertexId> selectedVertices,
                                std::span<const vn::geom::EdgeId> selectedEdges, const vn::geom::Constraint& constraint)
        -> bool {
    const bool vertexMatch =
            std::ranges::any_of(selectedVertices, [&constraint](vn::geom::VertexId id) {
                return std::ranges::find(constraint.vertices, id) != constraint.vertices.end();
            });
    const bool edgeMatch =
            std::ranges::any_of(selectedEdges, [&constraint](vn::geom::EdgeId id) {
                return std::ranges::find(constraint.edges, id) != constraint.edges.end();
            });
    return vertexMatch || edgeMatch;
}

static auto formatConstraintValue(double value) -> std::string {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(2);
    stream << value;
    auto text = stream.str();
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

static auto constraintBadgeText(const vn::geom::Constraint& constraint) -> std::string {
    using vn::geom::ConstraintKind;

    switch (constraint.kind) {
        case ConstraintKind::Coincident:
            return "COINC";
        case ConstraintKind::Horizontal:
            return "H";
        case ConstraintKind::Vertical:
            return "V";
        case ConstraintKind::Parallel:
            return "PAR";
        case ConstraintKind::Perpendicular:
            return "PERP";
        case ConstraintKind::FixedLength:
            return "L=" + formatConstraintValue(constraint.value);
        case ConstraintKind::EqualLength:
            return "EQ";
        case ConstraintKind::FixedAngle:
            return "ANGLE";
        case ConstraintKind::Radius:
            return "R";
        case ConstraintKind::OnEdge:
            return "ON";
    }

    return "C";
}

auto createFromFloatingElement(Control* ctrl, const PageRef& page, Layer* layer, PageView* view, ElementPtr eOwn)
        -> std::unique_ptr<EditSelection> {
    auto* e = eOwn.get();  // Order of parameter evaluation is unspecified, eOwn.get() must be evaluated before moving
    InsertionOrder i{1};
    i[0] = InsertionPosition{std::move(eOwn)};
    return std::make_unique<EditSelection>(ctrl, std::move(i), page, layer, view, Range(e->boundingRect()),
                                           Range(e->getSnappedBounds()));
}

auto createFromFloatingElements(Control* ctrl, const PageRef& page, Layer* layer, PageView* view,
                                InsertionOrder elts) -> std::pair<std::unique_ptr<EditSelection>, Range> {
    xoj_assert(std::is_sorted(elts.begin(), elts.end()));
    auto [bounds, snappingBounds] = computeBoxes(elts);
    return std::make_pair(
            std::make_unique<EditSelection>(ctrl, std::move(elts), page, layer, view, bounds, snappingBounds), bounds);
}

auto createFromElementOnActiveLayer(Control* ctrl, const PageRef& page, PageView* view, const Element* e,
                                    Element::Index pos) -> std::unique_ptr<EditSelection> {
    Document* doc = ctrl->getDocument();
    Layer* layer = nullptr;

    InsertionOrder i(1);
    i[0] = [&] {
        std::lock_guard lock(*doc);  // lock scope
        layer = page->getSelectedLayer();
        return layer->removeElementAt(e, pos);
    }();
    page->fireElementChanged(e);
    return std::make_unique<EditSelection>(ctrl, std::move(i), page, layer, view, Range(e->boundingRect()),
                                           Range(e->getSnappedBounds()));
}

auto createFromElementsOnActiveLayer(Control* ctrl, const PageRef& page, PageView* view, InsertionOrderRef elts)
        -> std::unique_ptr<EditSelection> {
    xoj_assert(std::is_sorted(elts.begin(), elts.end()));
    Document* doc = ctrl->getDocument();
    Layer* layer = nullptr;
    auto ownedElts = [&] {
        std::lock_guard lock(*doc);  // lock scope
        layer = page->getSelectedLayer();
        return layer->removeElementsAt(elts);
    }();

    auto [bounds, snappingBounds] = computeBoxes(ownedElts);
    page->fireRangeChanged(bounds);

    return std::make_unique<EditSelection>(ctrl, std::move(ownedElts), page, layer, view, bounds, snappingBounds);
}

auto addElementFromActiveLayer(Control* ctrl, EditSelection* base, const Element* e, Element::Index pos)
        -> std::unique_ptr<EditSelection> {
    Document* doc = ctrl->getDocument();
    Layer* layer = base->getSourceLayer();
    auto ownedElem = [&] {
        std::lock_guard lock(*doc);  // lock scope
        return layer->removeElementAt(e, pos);
    }();
    pos = ownedElem.pos;
    const PageRef& page = base->getSourcePage();
    page->fireElementChanged(e);

    InsertionOrder elts = base->makeMoveEffective();
    xoj_assert(!elts.empty());
    xoj_assert(std::is_sorted(elts.begin(), elts.end()));
    /**
     * To sort out the proper Element::Index of the added element *e,  we need to imagine elts were added to the layer,
     * so that the index may need to be increased.
     * Explicitly, we need to insert (e, pos + n) at position n so that the resulting vector is still sorted. Figuring
     * out the value of n requires our own binary search (std::lower_bound won't work).
     */
    auto begin = elts.begin(), first = begin, last = elts.end();
    while (first != last) {
        auto it = std::next(first, std::distance(first, last) / 2);
        if (it->pos <= pos + std::distance(begin, it)) {
            first = std::next(it);
        } else {
            last = it;
        }
    }
    ownedElem.pos += std::distance(begin, first);
    elts.insert(first, std::move(ownedElem));
    xoj_assert(std::is_sorted(elts.begin(), elts.end()));

    auto [bounds, snappingBounds] = computeBoxes(elts);

    return std::make_unique<EditSelection>(ctrl, std::move(elts), page, layer, base->getView(), bounds, snappingBounds);
}

auto addElementsFromActiveLayer(Control* ctrl, EditSelection* base, const InsertionOrderRef& newElts)
        -> std::unique_ptr<EditSelection> {
    xoj_assert(std::is_sorted(newElts.begin(), newElts.end()));
    Document* doc = ctrl->getDocument();
    Layer* layer = base->getSourceLayer();
    auto ownedElts = [&] {  // lock scope
        std::lock_guard lock(*doc);
        return layer->removeElementsAt(newElts);
    }();

    auto [bounds, snappingBounds] = computeBoxes(ownedElts);
    const PageRef& page = base->getSourcePage();
    page->fireRangeChanged(bounds);

    InsertionOrder oldElts = base->makeMoveEffective();
    xoj_assert(std::is_sorted(oldElts.begin(), oldElts.end()));
    auto [oldBounds, oldSnappingBounds] = computeBoxes(oldElts);

    InsertionOrder newSelection;
    newSelection.reserve(oldElts.size() + newElts.size());
    /**
     * To sort out the proper Element::Indices, we need to imagine oldElts were added back to the layer, so that some of
     * newElts' would see their indices increase. A simple std::merge won't do. See comment in addElementFromActiveLayer
     */
    auto oldIt = oldElts.begin(), oldEnd = oldElts.end();
    std::ptrdiff_t shift = 0;  // number of elements from oldElts that have been added to newSelection

    for (auto newIt = ownedElts.begin(), newEnd = ownedElts.end(); newIt != newEnd;) {
        if (oldIt == oldEnd) {
            xoj_assert(shift == static_cast<std::ptrdiff_t>(oldElts.size()));
            for (; newIt != newEnd; ++newIt) {
                newSelection.emplace_back(std::move(newIt->e), newIt->pos + shift);
            }
            break;
        }

        if (oldIt->pos < newIt->pos + shift) {
            newSelection.emplace_back(std::move(*oldIt));
            ++oldIt;
            ++shift;
        } else {
            newSelection.emplace_back(std::move(newIt->e), newIt->pos + shift);
            ++newIt;
        }
    }
    std::move(oldIt, oldEnd, std::back_inserter(newSelection));
    xoj_assert(newSelection.size() == oldElts.size() + newElts.size());
    xoj_assert(std::is_sorted(newSelection.begin(), newSelection.end()));
    return std::make_unique<EditSelection>(ctrl, std::move(newSelection), page, layer, base->getView(),
                                           bounds.unite(oldBounds), snappingBounds.unite(oldSnappingBounds));
}
};  // namespace SelectionFactory

static int getBtnWidth(Control* c) {
    return std::max(10, round_cast<int>(c->getZoomControl()->getZoom100Value() * Util::DPI_NORMALIZATION_FACTOR / 8));
}

EditSelection::EditSelection(Control* ctrl, InsertionOrder elts, const PageRef& page, Layer* layer, PageView* view,
                             const Range& bounds, const Range& snappingBounds):
        snappedBounds(snappingBounds),
        btnWidth(getBtnWidth(ctrl)),
        sourcePage(page),
        sourceLayer(layer),
        view(view),
        undo(ctrl->getUndoRedoHandler()),
        snappingHandler(ctrl->getSettings()) {
    snappingHandler.setPageRef(page);
    // make the visible bounding box large enough so that anchors do not collapse even for horizontal/vertical strokes
    x = bounds.minX - SELECTION_PADDING;
    y = bounds.minY - SELECTION_PADDING;
    width = bounds.getWidth() + 2 * SELECTION_PADDING;
    height = bounds.getHeight() + 2 * SELECTION_PADDING;

    this->contents = std::make_unique<EditSelectionContents>(this->getRect(), this->snappedBounds, this->sourcePage,
                                                             this->sourceLayer, this->view);
    this->contents->replaceInsertionOrder(std::move(elts));

    cairo_matrix_init_identity(&this->cmatrix);
    this->view->getNoteView()->getCursor()->setRotationAngle(0);
    this->view->getNoteView()->getCursor()->setMirror(false);

    for (const auto& e: contents->getElementsView()) {
        this->preserveAspectRatio = this->preserveAspectRatio || e->rescaleOnlyAspectRatio();
        this->supportMirroring = this->supportMirroring && e->rescaleWithMirror();
        this->supportRotation = this->supportRotation && e->getType() == ELEMENT_STROKE;
    }
}


EditSelection::EditSelection(Control* ctrl, const PageRef& page, Layer* layer, PageView* view):
        snappedBounds(Rectangle<double>{}),
        btnWidth(getBtnWidth(ctrl)),
        sourcePage(page),
        sourceLayer(layer),
        view(view),
        undo(ctrl->getUndoRedoHandler()),
        snappingHandler(ctrl->getSettings()) {
    snappingHandler.setPageRef(page);
}

EditSelection::~EditSelection() {
    this->edgePanHandler.cancel();
    finalizeSelection();
}

/**
 * Finishes all pending changes, move the elements, scale the elements and add
 * them to new layer if any or to the old if no new layer
 */
void EditSelection::finalizeSelection() {
    PageView* v = getPageViewUnderCursor();
    if (v == nullptr) {  // Not on any page - move back to original page and position
        double ox = this->snappedBounds.x - this->x;
        double oy = this->snappedBounds.y - this->y;
        this->x = this->contents->getOriginalX();
        this->y = this->contents->getOriginalY();
        this->snappedBounds.x = this->x + ox;
        this->snappedBounds.y = this->y + oy;
        v = this->contents->getSourceView();

        PageRef page = v->getPage();
        Layer* layer = page->getSelectedLayer();
        // Create an Undo action to compensate - avoids Segfault/Freeze if the user presses undo after this happened
        this->contents->updateContent(this->getRect(), this->snappedBounds, this->rotation, this->preserveAspectRatio,
                                      layer, page, this->undo, CURSOR_SELECTION_MOVE);
    }


    this->view = v;

    auto insertOrder =
            this->contents->makeMoveEffective(this->getRect(), this->snappedBounds, this->preserveAspectRatio);


    auto* doc = view->getNoteView()->getControl()->getDocument();
    doc->lock();

    Layer* destinationLayer = this->view->getPage()->getSelectedLayer();
    for (auto&& [e, index]: insertOrder) {
        if (index == Element::InvalidIndex) {
            // if the element didn't have a source layer (e.g, clipboard)
            destinationLayer->addElement(std::move(e));
        } else {
            destinationLayer->insertElement(std::move(e), index);
        }
    }
    doc->unlock();


    // Calculate new clip region delta due to rotation:
    double addW =
            std::abs(this->width * cos(this->rotation)) + std::abs(this->height * sin(this->rotation)) - this->width;
    double addH =
            std::abs(this->width * sin(this->rotation)) + std::abs(this->height * cos(this->rotation)) - this->height;


    this->view->rerenderRect(this->x - addW / 2.0, this->y - addH / 2.0, this->width + addW, this->height + addH);

    // This is needed if the selection not was 100% on a page
    this->view->getNoteView()->repaintSelection(true);
}

auto EditSelection::makeMoveEffective() -> InsertionOrder {
    return contents->makeMoveEffective(this->getRect(), this->snappedBounds, this->preserveAspectRatio);
}


/**
 * get the X coordinate relative to the provided view (getView())
 * in document coordinates
 */
auto EditSelection::getXOnView() const -> double { return this->x; }

/**
 * get the Y coordinate relative to the provided view (getView())
 * in document coordinates
 */
auto EditSelection::getYOnView() const -> double { return this->y; }

auto EditSelection::getOriginalXOnView() -> double { return this->contents->getOriginalX(); }

auto EditSelection::getOriginalYOnView() -> double { return this->contents->getOriginalY(); }

/**
 * get the width in document coordinates (multiple with zoom)
 */
auto EditSelection::getWidth() const -> double { return this->width; }

/**
 * get the height in document coordinates (multiple with zoom)
 */
auto EditSelection::getHeight() const -> double { return this->height; }

/**
 * get the bounding rectangle in document coordinates (multiple with zoom)
 */
auto EditSelection::getRect() const -> Rectangle<double> {
    return Rectangle<double>{this->x, this->y, this->width, this->height};
}

/**
 * gets the minimal bounding box containing all elements of the selection used for e.g. grid snapping
 */
auto EditSelection::getSnappedBounds() const -> Rectangle<double> { return Rectangle<double>{this->snappedBounds}; }

/**
 * get the original bounding rectangle in document coordinates
 */
auto EditSelection::getOriginalBounds() const -> Rectangle<double> {
    return Rectangle<double>{this->contents->getOriginalBounds()};
}

/**
 * Get the rotation angle of the selection
 */
auto EditSelection::getRotation() const -> double { return this->rotation; }

/**
 * Get if the selection supports being rotated
 */
auto EditSelection::isRotationSupported() const -> bool { return this->supportRotation; }

/**
 * Get the source page (where the selection was done)
 */
auto EditSelection::getSourcePage() const -> PageRef { return this->sourcePage; }

/**
 * Get the source layer (form where the Elements come)
 */
auto EditSelection::getSourceLayer() const -> Layer* { return this->sourceLayer; }

/**
 * Sets the tool size for pen or eraser, returs an undo action
 * (or nullptr if nothing is done)
 */
auto EditSelection::setSize(ToolSize size, const double* thicknessPen, const double* thicknessHighlighter,
                            const double* thicknessEraser) -> UndoActionPtr {
    return this->contents->setSize(size, thicknessPen, thicknessHighlighter, thicknessEraser);
}

/**
 * Fills the stroke, return an undo action
 * (Or nullptr if nothing done, e.g. because there is only an image)
 */
auto EditSelection::setFill(int alphaPen, int alphaHighligther) -> UndoActionPtr {
    return this->contents->setFill(alphaPen, alphaHighligther);
}

/**
 * Set the line style of all elements, return an undo action
 * (Or nullptr if nothing done)
 */
auto EditSelection::setLineStyle(LineStyle style) -> UndoActionPtr { return this->contents->setLineStyle(style); }

/**
 * Set the color of all elements, return an undo action
 * (Or nullptr if nothing done, e.g. because there is only an image)
 */
auto EditSelection::setColor(Color color) -> UndoActionPtr { return this->contents->setColor(color); }

/**
 * Sets the font of all containing text elements, return an undo action
 * (or nullptr if there are no Text elements)
 */
auto EditSelection::setFont(const NoteFont& font) -> UndoActionPtr { return this->contents->setFont(font); }

/**
 * Fills de undo item if the selection is deleted
 * the selection is cleared after
 */
void EditSelection::fillUndoItem(DeleteUndoAction* undo) { this->contents->fillUndoItem(undo); }

/**
 * Add an element to this selection
 *
 */
void EditSelection::addElement(ElementPtr eOwned, Element::Index order) {
    auto e = eOwned.get();
    this->contents->addElement(std::move(eOwned), order);
    this->preserveAspectRatio = this->preserveAspectRatio || e->rescaleOnlyAspectRatio();
    this->supportMirroring = this->supportMirroring && e->rescaleWithMirror();
    this->supportRotation = this->supportRotation && e->getType() == ELEMENT_STROKE;
}

/**
 * Returns all containing elements of this selection
 */
auto EditSelection::getElementsView() const -> vn::util::PointerContainerView<std::vector<Element*>> {
    return this->contents->getElementsView();
}

void EditSelection::forEachElement(std::function<void(const Element*)> f) const {
    this->contents->forEachElement(std::move(f));
}

/**
 * Returns the insert order of this selection
 */
auto EditSelection::getInsertionOrder() const -> const InsertionOrder& { return this->contents->getInsertionOrder(); }

auto EditSelection::rearrangeInsertionOrder(const OrderChange change) -> UndoActionPtr {
    InsertionOrder orderOwned = this->contents->stealInsertionOrder();
    auto oldOrd = refInsertionOrder(orderOwned);
    std::string desc = _("Arrange");
    switch (change) {
        case OrderChange::BringToFront:
            for (auto& [_, i]: orderOwned) {
                i = std::numeric_limits<Element::Index>::max();
            }
            break;
        case OrderChange::BringForward:
            // Set indices of elements to range from [max(indices) + 1, max(indices) + 1 + num elements)
            if (!orderOwned.empty()) {
                Element::Index i = orderOwned.back().pos + 1;
                for (auto& [_, pos]: orderOwned) {
                    pos = i++;
                }
            }
            desc = _("Bring forward");
            break;
        case OrderChange::SendBackward:
            // Set indices of elements to range from [min(indices) - 1, min(indices) + num elements - 1)
            if (!orderOwned.empty()) {
                Element::Index i = orderOwned.front().pos;
                i = i > 0 ? i - 1 : 0;
                for (auto& [_, pos]: orderOwned) {
                    pos = i++;
                }
            }
            desc = _("Send backward");
            break;
        case OrderChange::SendToBack:
            Element::Index i = 0;
            for (auto& [_, pos]: orderOwned) {
                pos = i++;
            }
            desc = _("Send to back");
            break;
    }


    auto newOrd = refInsertionOrder(orderOwned);
    this->contents->replaceInsertionOrder(std::move(orderOwned));
    PageRef page = this->view->getPage();

    return std::make_unique<ArrangeUndoAction>(page, page->getSelectedLayer(), desc, std::move(oldOrd),
                                               std::move(newOrd));
}

/**
 * Finish the current movement
 * (should be called in the mouse-button-released event handler)
 */
void EditSelection::mouseUp() {
    if (this->mouseDownType == CURSOR_SELECTION_DELETE) {
        this->view->getNoteView()->deleteSelection();
        return;
    }

    if (this->mouseDownType == CURSOR_SELECTION_GEOMETRY_VERTEX) {
        if (this->activeGeometryElement && this->activeGeometryVertexMoved &&
            this->activeGeometryVertices.size() == this->activeGeometryVertexCurrentPositions.size()) {
            const bool useTopologyUndo = this->activeGeometryBeforeDrag &&
                                         !this->activeGeometryBeforeDrag->constraints().empty();
            if (useTopologyUndo) {
                this->undo->addUndoAction(std::make_unique<GeometryTopologyUndoAction>(
                        this->sourcePage, this->activeGeometryElement, *this->activeGeometryBeforeDrag,
                        this->activeGeometryElement->geometry(),
                        this->activeGeometryVertices.size() > 1U ? _("Move constrained geometry vertices")
                                                                 : _("Move constrained geometry vertex")));
            } else if (this->activeGeometryVertices.size() > 1U) {
                this->undo->addUndoAction(std::make_unique<GeometryVertexMoveUndoAction>(
                        this->sourcePage, this->activeGeometryElement, this->activeGeometryVertices,
                        this->activeGeometryVertexStartPositions, this->activeGeometryVertexCurrentPositions));
            } else {
                this->undo->addUndoAction(std::make_unique<GeometryVertexMoveUndoAction>(
                        this->sourcePage, this->activeGeometryElement, this->activeGeometryVertex,
                        this->activeGeometryVertexStart, this->activeGeometryVertexCurrent));
            }
            rebaseSelectionBounds();
            this->contents->invalidateViewBuffer();
            this->view->getPage()->fireElementChanged(this->activeGeometryElement);
        }

        this->activeGeometryVertexMoved = false;
        this->mouseDownType = CURSOR_SELECTION_NONE;
        clearGeometrySnapState();
        this->view->getNoteView()->repaintSelection();
        return;
    }

    if (this->mouseDownType == CURSOR_SELECTION_GEOMETRY_EDGE) {
        this->mouseDownType = CURSOR_SELECTION_NONE;
        this->view->getNoteView()->repaintSelection();
        return;
    }


    PageRef page = this->view->getPage();
    Layer* layer = page->getSelectedLayer();
    this->rotation = snappingHandler.snapAngle(this->rotation, false);

    this->sourcePage = page;
    this->sourceLayer = layer;

    this->contents->updateContent(this->getRect(), this->snappedBounds, this->rotation, this->preserveAspectRatio,
                                  layer, page, this->undo, this->mouseDownType);

    this->mouseDownType = CURSOR_SELECTION_NONE;

    const bool wasEdgePanning = this->isEdgePanning();
    this->setEdgePan(false);
    updateMatrix();
    if (wasEdgePanning) {
        this->ensureWithinVisibleArea();
    }
}

void EditSelection::mouseDown(CursorSelectionType type, double x, double y, bool shiftDown) {
    double zoom = this->view->getNoteView()->getZoom();

    this->mouseDownType = type;
    if (type != CURSOR_SELECTION_GEOMETRY_VERTEX && type != CURSOR_SELECTION_GEOMETRY_EDGE) {
        clearGeometryVertexSelection();
        this->hoveredGeometryElement = nullptr;
        this->hoveredGeometryEdge = vn::geom::InvalidEdgeId;
    } else if (this->hoveredGeometryVertexElement && this->hoveredGeometryVertex != vn::geom::InvalidVertexId) {
        if (shiftDown) {
            toggleGeometryVertexSelection(this->hoveredGeometryVertexElement, this->hoveredGeometryVertex,
                                          this->hoveredGeometryVertexPosition);
        } else if (!isGeometryVertexSelected(this->hoveredGeometryVertexElement, this->hoveredGeometryVertex)) {
            setSingleGeometryVertexSelection(this->hoveredGeometryVertexElement, this->hoveredGeometryVertex,
                                             this->hoveredGeometryVertexPosition);
        }

        if (this->activeGeometryElement && !this->activeGeometryVertices.empty()) {
            this->activeGeometryVertexStartPositions = this->activeGeometryVertexCurrentPositions;
            this->activeGeometryVertexStart = this->activeGeometryVertexCurrent;
            if (const auto index = findSelectedGeometryVertex(this->activeGeometryElement, this->activeGeometryVertex)) {
                this->activeGeometryVertexStart = this->activeGeometryVertexCurrentPositions[*index];
                this->activeGeometryVertexCurrent = this->activeGeometryVertexCurrentPositions[*index];
            }
            this->activeGeometryVertexMoved = false;
            rebuildGeometrySnapEngine();
            this->activeGeometryBeforeDrag = this->activeGeometryElement->geometry();
        }
    } else if (type == CURSOR_SELECTION_GEOMETRY_EDGE) {
        if (this->hoveredGeometryElement && this->hoveredGeometryEdge != vn::geom::InvalidEdgeId) {
            if (shiftDown) {
                toggleGeometryEdgeSelection(this->hoveredGeometryElement, this->hoveredGeometryEdge);
            } else if (!isGeometryEdgeSelected(this->hoveredGeometryElement, this->hoveredGeometryEdge)) {
                setSingleGeometryEdgeSelection(this->hoveredGeometryElement, this->hoveredGeometryEdge);
            }
        }
        clearGeometrySnapState();
    }

    // coordinates relative to top left corner of snapped bounds in coordinate system which is not modified
    this->relMousePosX = x / zoom - this->snappedBounds.x;
    this->relMousePosY = y / zoom - this->snappedBounds.y;

    // coordinates relative to top left corner of snapped bounds in coordinate system which is rotated to make bounding
    // box edges horizontal/vertical
    cairo_matrix_transform_point(&this->cmatrix, &x, &y);
    this->relMousePosRotX = x / zoom - this->snappedBounds.x;
    this->relMousePosRotY = y / zoom - this->snappedBounds.y;
}

void EditSelection::mouseMove(double mouseX, double mouseY, bool alt) {
    double zoom = this->view->getNoteView()->getZoom();

    if (this->mouseDownType == CURSOR_SELECTION_GEOMETRY_VERTEX) {
        if (!this->activeGeometryElement || this->activeGeometryVertex == vn::geom::InvalidVertexId ||
            this->activeGeometryVertices.empty()) {
            return;
        }

        double transformedX = mouseX;
        double transformedY = mouseY;
        cairo_matrix_transform_point(&this->cmatrix, &transformedX, &transformedY);
        auto modelPosition = geometryVertexPreviewToModel(transformedX / zoom, transformedY / zoom);
        modelPosition = snapGeometryVertexDragPosition(modelPosition, alt, zoom);

        const vn::geom::Vec2 delta{modelPosition.x - this->activeGeometryVertexStart.x,
                                   modelPosition.y - this->activeGeometryVertexStart.y};

        bool changed = false;
        this->activeGeometryVertexCurrent = modelPosition;
        this->activeGeometryVertexCurrentPositions.clear();
        this->activeGeometryVertexCurrentPositions.reserve(this->activeGeometryVertexStartPositions.size());

        for (std::size_t i = 0; i < this->activeGeometryVertices.size(); ++i) {
            const auto start = this->activeGeometryVertexStartPositions[i];
            const vn::geom::Vec2 next{start.x + delta.x, start.y + delta.y};
            changed = this->activeGeometryElement->setVertexPosition(this->activeGeometryVertices[i], next) || changed;
            this->activeGeometryVertexCurrentPositions.push_back(next);
        }
        changed = applyGeometryConstraints(this->activeGeometryElement->geometry()) || changed;

        for (std::size_t i = 0; i < this->activeGeometryVertices.size(); ++i) {
            if (const auto* vertex = this->activeGeometryElement->geometry().vertex(this->activeGeometryVertices[i])) {
                this->activeGeometryVertexCurrentPositions[i] = vertex->position;
                if (this->activeGeometryVertices[i] == this->activeGeometryVertex) {
                    this->activeGeometryVertexCurrent = vertex->position;
                }
            }
        }

        if (changed) {
            this->activeGeometryVertexMoved =
                    delta.x != 0.0 || delta.y != 0.0 || this->activeGeometryVertices.size() > 1U;
            this->contents->invalidateViewBuffer();
            this->view->getNoteView()->repaintSelection();
        }
        return;
    }

    if (this->mouseDownType == CURSOR_SELECTION_MOVE) {
        // compute translation (without snapping)
        double dx = mouseX / zoom - this->snappedBounds.x - this->relMousePosX;
        double dy = mouseY / zoom - this->snappedBounds.y - this->relMousePosY;

        // find corner of reduced bounding box in rotated coordinate system closest to grabbing position
        double cx = this->snappedBounds.x;
        double cy = this->snappedBounds.y;
        if ((this->relMousePosRotX > this->snappedBounds.width / 2) ==
            (this->snappedBounds.width > 0)) {  // closer to the right side
            cx += this->snappedBounds.width;
        }
        if ((this->relMousePosRotY > this->snappedBounds.height / 2) ==
            (this->snappedBounds.height > 0)) {  // closer to the lower side
            cy += this->snappedBounds.height;
        }

        // compute corner of reduced bounding box in unmodified coordinate system closest to grabbing position
        cairo_matrix_t inv = this->cmatrix;
        cairo_matrix_invert(&inv);
        cx *= zoom;
        cy *= zoom;
        cairo_matrix_transform_point(&inv, &cx, &cy);
        cx /= zoom;
        cy /= zoom;

        // compute position where unsnapped corner would move
        Point p = Point(cx + dx, cy + dy);

        // snap this corner
        p = snappingHandler.snapToGrid(p, alt);

        // move
        if (!this->edgePanInhibitNext) {
            moveSelection(p.x - cx, p.y - cy);
            this->setEdgePan(true);
        } else {
            this->edgePanInhibitNext = false;
        }
    } else if (this->mouseDownType == CURSOR_SELECTION_ROTATE && supportRotation) {  // catch rotation here
        double rdx = mouseX / zoom - this->snappedBounds.x - this->snappedBounds.width / 2;
        double rdy = mouseY / zoom - this->snappedBounds.y - this->snappedBounds.height / 2;

        double angle = atan2(rdy, rdx);
        this->rotation = angle;
        this->view->getNoteView()->getCursor()->setRotationAngle(180 / M_PI * angle);
    } else {
        // Translate mouse position into rotated coordinate system:
        double rx = mouseX;
        double ry = mouseY;
        cairo_matrix_transform_point(&this->cmatrix, &rx, &ry);
        rx /= zoom;
        ry /= zoom;

        double minSize = MINPIXSIZE / zoom;

        // store pull direction value
        int xSide = 0;
        int ySide = 0;
        if (this->mouseDownType == CURSOR_SELECTION_TOP_LEFT) {
            xSide = -1;
            ySide = -1;
        } else if (this->mouseDownType == CURSOR_SELECTION_TOP_RIGHT) {
            xSide = 1;
            ySide = -1;
        } else if (this->mouseDownType == CURSOR_SELECTION_BOTTOM_LEFT) {
            xSide = -1;
            ySide = 1;
        } else if (this->mouseDownType == CURSOR_SELECTION_BOTTOM_RIGHT) {
            xSide = 1;
            ySide = 1;
        } else if (this->mouseDownType == CURSOR_SELECTION_TOP) {
            ySide = -1;
        } else if (this->mouseDownType == CURSOR_SELECTION_BOTTOM) {
            ySide = 1;
        } else if (this->mouseDownType == CURSOR_SELECTION_LEFT) {
            xSide = -1;
        } else if (this->mouseDownType == CURSOR_SELECTION_RIGHT) {
            xSide = 1;
        }
        // sanity check
        if (xSide || ySide) {
            // get normalized direction vector for input interpretation (dependent on aspect ratio)
            double diag = hypot(xSide * this->width, ySide * this->height);
            double nx = xSide * this->width / diag;
            double ny = ySide * this->height / diag;

            int xMul = (xSide + 1) / 2;
            int yMul = (ySide + 1) / 2;
            double xOffset =
                    (rx - this->x) - this->width * xMul;  // x-offset from corner/side that is used for resizing
            double yOffset =
                    (ry - this->y) - this->height * yMul;  // y-offset from corner/side that is used for resizing

            // calculate scale factor using dot product
            double f = (xOffset * nx + yOffset * ny + diag) / diag;
            f = std::copysign(std::max(std::abs(f), minSize / std::min(std::abs(this->width), std::abs(this->height))),
                              f);
            if (supportMirroring || f > 0) {
                scaleShift(xSide ? f : 1, ySide ? f : 1, xSide == -1, ySide == -1);

                // in each case first scale without snapping consideration then snap
                // take care that wSnap and hSnap are not too small
                double snappedX =
                        snappingHandler.snapHorizontally(this->snappedBounds.x + this->snappedBounds.width * xMul, alt);
                double snappedY =
                        snappingHandler.snapVertically(this->snappedBounds.y + this->snappedBounds.height * yMul, alt);
                double dx = snappedX - this->snappedBounds.x - this->snappedBounds.width * xMul;
                double dy = snappedY - this->snappedBounds.y - this->snappedBounds.height * yMul;
                double fx = (std::abs(this->snappedBounds.width) > minSize) ?
                                    (this->snappedBounds.width + dx * xSide) / this->snappedBounds.width :
                                    1;
                double fy = (std::abs(this->snappedBounds.height) > minSize) ?
                                    (this->snappedBounds.height + dy * ySide) / this->snappedBounds.height :
                                    1;
                f = (((std::abs(dx) < std::abs(dy)) && (fx != 1)) || fy == 1) ? fx : fy;
                f = (std::abs(this->width) * std::abs(f) < minSize || std::abs(this->height) * std::abs(f) < minSize) ?
                            1 :
                            f;
                scaleShift(xSide ? f : 1, ySide ? f : 1, xSide == -1, ySide == -1);

                this->view->getNoteView()->getCursor()->setMirror(this->width * this->height < 0);
            }
        }
    }

    this->view->getNoteView()->repaintSelection();

    if (this->mouseDownType == CURSOR_SELECTION_MOVE) {
        PageView* v = getPageViewUnderCursor();

        if (v && v != this->view) {
            VertexNoteView* noteView = this->view->getNoteView();
            const auto pageNr = noteView->getControl()->getDocument()->indexOf(v->getPage());

            noteView->pageSelected(pageNr);

            translateToView(v);
        }
    }
}

// scales with scale factors fx and fy fixing the corner of the reduced bounding box defined by changeLeft and
// changeTop
void EditSelection::scaleShift(double fx, double fy, bool changeLeft, bool changeTop) {
    double dx = (changeLeft) ? this->snappedBounds.width * (1 - fx) : 0;
    double dy = (changeTop) ? this->snappedBounds.height * (1 - fy) : 0;
    this->width *= fx;
    this->height *= fy;
    this->snappedBounds.width *= fx;
    this->snappedBounds.height *= fy;

    this->x += dx + (this->x - this->snappedBounds.x) * (fx - 1);
    this->y += dy + (this->y - this->snappedBounds.y) * (fy - 1);
    this->snappedBounds.x += dx;
    this->snappedBounds.y += dy;

    // compute new rotation center
    double cx = this->snappedBounds.x + this->snappedBounds.width / 2;
    double cy = this->snappedBounds.y + this->snappedBounds.height / 2;
    // transform it back with old rotation center
    double zoom = this->view->getNoteView()->getZoom();
    double cxRot = cx * zoom;
    double cyRot = cy * zoom;
    cairo_matrix_t inv = this->cmatrix;
    cairo_matrix_invert(&inv);
    cairo_matrix_transform_point(&inv, &cxRot, &cyRot);
    cxRot /= zoom;
    cyRot /= zoom;
    // move to compensate for changed rotation centers
    moveSelection(cxRot - cx, cyRot - cy);
}

auto EditSelection::getPageViewUnderCursor() -> PageView* {
    double zoom = view->getNoteView()->getZoom();

    // get grabbing hand position
    auto p = this->view->getPixelPosition();
    double hx = p.x + (this->snappedBounds.x + this->relMousePosX) * zoom;
    double hy = p.y + (this->snappedBounds.y + this->relMousePosY) * zoom;


    Layout* layout = this->view->getNoteView()->getLayout();
    PageView* v = layout->getPageViewAt(static_cast<int>(hx), static_cast<int>(hy));

    return v;
}

/**
 * Translate all coordinates which are relative to the current view to the new view,
 * and set the attribute view to the new view
 */
void EditSelection::translateToView(PageView* v) {
    double zoom = view->getNoteView()->getZoom();

    double ox = this->snappedBounds.x - this->x;
    double oy = this->snappedBounds.y - this->y;

    auto diff = this->view->getPixelPosition() - v->getPixelPosition();

    this->x += diff.x / zoom;
    this->y += diff.y / zoom;
    this->snappedBounds.x = this->x + ox;
    this->snappedBounds.y = this->y + oy;

    this->view = v;
}

void EditSelection::copySelection() {
    // clone elements in the insert order
    auto const& orig = getInsertionOrder();
    InsertionOrder clonedInsertionOrder;
    clonedInsertionOrder.reserve(orig.size());
    for (const auto& [e, index]: orig) {
        clonedInsertionOrder.emplace_back(e->clone(), index);
    }

    // apply transformations and add to layer
    finalizeSelection();

    // restore insert order
    contents->replaceInsertionOrder(std::move(clonedInsertionOrder));

    // add undo action
    PageRef page = this->view->getPage();
    Layer* layer = page->getSelectedLayer();
    undo->addUndoAction(std::make_unique<InsertsUndoAction>(page, layer, getElementsView().clone()));
}

/**
 * If the selection should moved (or rescaled)
 */
auto EditSelection::isMoving() const -> bool { return this->mouseDownType != CURSOR_SELECTION_NONE; }

auto EditSelection::isDeleting() const -> bool { return this->mouseDownType == CURSOR_SELECTION_DELETE; }


/**
 * Move the selection
 */

void EditSelection::updateMatrix() {
    double zoom = this->view->getNoteView()->getZoom();
    // store rotation matrix for pointer use; the center of the rotation is the center of the bounding box
    double rx = (this->snappedBounds.x + this->snappedBounds.width / 2) * zoom;
    double ry = (this->snappedBounds.y + this->snappedBounds.height / 2) * zoom;

    cairo_matrix_init_identity(&this->cmatrix);
    cairo_matrix_translate(&this->cmatrix, rx, ry);
    cairo_matrix_rotate(&this->cmatrix, -this->rotation);
    cairo_matrix_translate(&this->cmatrix, -rx, -ry);
}

void EditSelection::rebaseSelectionBounds() {
    std::optional<Range> bounds;
    std::optional<Range> snappingBounds;

    for (const auto* element: this->contents->getElementsView()) {
        if (!bounds) {
            bounds = Range(element->boundingRect());
            snappingBounds = Range(element->getSnappedBounds());
            continue;
        }

        bounds = bounds->unite(Range(element->boundingRect()));
        snappingBounds = snappingBounds->unite(Range(element->getSnappedBounds()));
    }

    if (!bounds || !snappingBounds) {
        return;
    }

    this->x = bounds->minX - SELECTION_PADDING;
    this->y = bounds->minY - SELECTION_PADDING;
    this->width = bounds->getWidth() + 2 * SELECTION_PADDING;
    this->height = bounds->getHeight() + 2 * SELECTION_PADDING;
    this->snappedBounds = Rectangle<double>(*snappingBounds);
    this->contents->rebaseBounds(this->getRect(), this->snappedBounds);
    updateMatrix();
}

void EditSelection::clearGeometrySnapState() {
    this->activeGeometrySnapKind.reset();
    this->activeGeometrySnapPoint = {};
}

void EditSelection::rebuildGeometrySnapEngine() {
    clearGeometrySnapState();
    this->geometrySnapProvider.reset();
    this->geometrySnapEngine.clearProviders();

    const auto* settings = this->view->getNoteView()->getControl()->getSettings();
    if (!settings->isVertexNoteGeometrySnapEnabled() || !this->sourcePage || !this->activeGeometryElement) {
        return;
    }

    auto objects = vn::snap::collectGeometryObjects(this->sourcePage);
    const auto activeObjectId = this->activeGeometryElement->geometry().objectId();
    objects.erase(std::remove_if(objects.begin(), objects.end(),
                                 [activeObjectId](const vn::geom::GeometryObject* object) {
                                     return !object || object->objectId() == activeObjectId;
                                 }),
                  objects.end());

    if (objects.empty()) {
        return;
    }

    this->geometrySnapProvider = std::make_shared<vn::snap::GeometrySnapProvider>(std::move(objects));
    this->geometrySnapEngine.addProvider(this->geometrySnapProvider);
}

auto EditSelection::snapGeometryVertexDragPosition(vn::geom::Vec2 modelPosition, bool alt, double zoom)
        -> vn::geom::Vec2 {
    clearGeometrySnapState();

    if (this->geometrySnapProvider) {
        const auto geometrySnap =
                this->geometrySnapEngine.snap(vn::snap::SnapQuery{modelPosition, zoom, GEOMETRY_SNAP_RADIUS_PIXELS});
        if (geometrySnap.snapped()) {
            this->activeGeometrySnapKind = geometrySnap.candidate->kind;
            this->activeGeometrySnapPoint = geometrySnap.pagePoint;
            return geometrySnap.pagePoint;
        }
    }

    const auto* settings = this->view->getNoteView()->getControl()->getSettings();
    if (!settings->isVertexNoteGridSnapEnabled()) {
        return modelPosition;
    }

    Point snapped(modelPosition.x, modelPosition.y);
    snapped = this->snappingHandler.snapToGrid(snapped, alt);
    if (snapped.x != modelPosition.x || snapped.y != modelPosition.y) {
        this->activeGeometrySnapKind = vn::snap::SnapKind::Grid;
        this->activeGeometrySnapPoint = {snapped.x, snapped.y};
    }

    return {snapped.x, snapped.y};
}

auto EditSelection::applyGeometryConstraints(vn::geom::GeometryObject& object) const -> bool {
    if (object.constraints().empty()) {
        return false;
    }

    const vn::constraints::GeometryConstraintSolver solver;
    return solver.apply(object).changed;
}

void EditSelection::moveSelection(double dx, double dy, bool addMoveUndo) {
    this->x += dx;
    this->y += dy;
    this->snappedBounds.x += dx;
    this->snappedBounds.y += dy;

    updateMatrix();

    if (addMoveUndo) {
        PageView* v = getPageViewUnderCursor();

        if (v && v != this->view) {
            VertexNoteView* noteView = this->view->getNoteView();
            const auto pageNr = noteView->getControl()->getDocument()->indexOf(v->getPage());

            noteView->pageSelected(pageNr);

            translateToView(v);
        }
        this->contents->updateContent(this->getRect(), this->snappedBounds, this->rotation, this->preserveAspectRatio,
                                      this->view->getPage()->getSelectedLayer(), this->view->getPage(), this->undo,
                                      CURSOR_SELECTION_MOVE);
    }

    this->view->getNoteView()->repaintSelection();
}

auto EditSelection::deleteActiveGeometryVertex() -> bool {
    if (!this->activeGeometryElement || this->activeGeometryVertices.empty()) {
        return false;
    }

    const auto before = this->activeGeometryElement->geometry();
    auto after = before;
    bool changed = false;
    for (const auto vertex: this->activeGeometryVertices) {
        changed = after.removeVertex(vertex) || changed;
    }
    changed = applyGeometryConstraints(after) || changed;

    if (!changed) {
        return false;
    }

    this->activeGeometryElement->replaceGeometry(after);
    this->undo->addUndoAction(std::make_unique<GeometryTopologyUndoAction>(
            this->sourcePage, this->activeGeometryElement, before, after,
            this->activeGeometryVertices.size() > 1U ? _("Delete geometry vertices") : _("Delete geometry vertex")));
    clearGeometryVertexSelection();
    this->hoveredGeometryElement = nullptr;
    this->hoveredGeometryEdge = vn::geom::InvalidEdgeId;
    rebaseSelectionBounds();
    this->contents->invalidateViewBuffer();
    this->view->getPage()->fireElementChanged(this->activeGeometryElement);
    this->view->getNoteView()->repaintSelection();
    return true;
}

auto EditSelection::insertActiveGeometryVertexOnEdge() -> bool {
    if (!this->hoveredGeometryElement || this->hoveredGeometryEdge == vn::geom::InvalidEdgeId) {
        return false;
    }

    const auto before = this->hoveredGeometryElement->geometry();
    auto after = before;
    auto inserted = after.insertVertexOnEdge(this->hoveredGeometryEdge, this->hoveredGeometryInsertPosition);
    if (!inserted) {
        return false;
    }
    (void) applyGeometryConstraints(after);
    this->hoveredGeometryElement->replaceGeometry(after);
    this->undo->addUndoAction(std::make_unique<GeometryTopologyUndoAction>(
            this->sourcePage, this->hoveredGeometryElement, before, after, _("Insert geometry vertex")));

    setSingleGeometryVertexSelection(this->hoveredGeometryElement, *inserted, this->hoveredGeometryInsertPosition);
    this->hoveredGeometryEdge = vn::geom::InvalidEdgeId;
    rebaseSelectionBounds();
    this->contents->invalidateViewBuffer();
    this->view->getPage()->fireElementChanged(this->activeGeometryElement);
    this->view->getNoteView()->repaintSelection();
    return true;
}

auto EditSelection::insertGeometryVertexAt(double x, double y, double zoom) -> bool {
    cairo_matrix_transform_point(&this->cmatrix, &x, &y);
    if (!selectGeometryEdgeAt(x, y, zoom)) {
        return false;
    }

    return insertActiveGeometryVertexOnEdge();
}

auto EditSelection::applyGeometryConstraint(vn::geom::ConstraintKind kind) -> bool {
    if (!this->activeGeometryElement) {
        return false;
    }

    auto before = this->activeGeometryElement->geometry();
    auto after = before;

    try {
        switch (kind) {
            case vn::geom::ConstraintKind::Coincident:
                if (this->activeGeometryVertices.size() < 2U) {
                    return false;
                }
                after.addConstraint(kind, this->activeGeometryVertices);
                break;
            case vn::geom::ConstraintKind::Horizontal:
            case vn::geom::ConstraintKind::Vertical:
                if (this->activeGeometryVertices.size() != 2U) {
                    return false;
                }
                after.addConstraint(kind, {this->activeGeometryVertices[0], this->activeGeometryVertices[1]});
                break;
            case vn::geom::ConstraintKind::FixedLength: {
                if (this->activeGeometryVertices.size() != 2U) {
                    return false;
                }
                const vn::geom::Constraint preview{vn::geom::InvalidConstraintId, kind,
                                                   {this->activeGeometryVertices[0], this->activeGeometryVertices[1]},
                                                   {}, 0.0};
                const double length = SelectionFactory::edgeLength(before, preview);
                if (length <= 0.0) {
                    return false;
                }
                after.addConstraint(kind, {this->activeGeometryVertices[0], this->activeGeometryVertices[1]}, {},
                                    length);
                break;
            }
            case vn::geom::ConstraintKind::Parallel:
            case vn::geom::ConstraintKind::Perpendicular:
                if (this->activeGeometryEdges.size() != 2U) {
                    return false;
                }
                after.addConstraint(kind, {}, {this->activeGeometryEdges[0], this->activeGeometryEdges[1]});
                break;
            case vn::geom::ConstraintKind::EqualLength:
            case vn::geom::ConstraintKind::FixedAngle:
            case vn::geom::ConstraintKind::Radius:
            case vn::geom::ConstraintKind::OnEdge:
                return false;
        }
    } catch (const std::invalid_argument&) {
        return false;
    }

    const bool _solverChanged = applyGeometryConstraints(after);
    (void) _solverChanged;
    this->activeGeometryElement->replaceGeometry(after);
    this->undo->addUndoAction(std::make_unique<GeometryTopologyUndoAction>(
            this->sourcePage, this->activeGeometryElement, before, after, _("Create geometry constraint")));
    this->contents->invalidateViewBuffer();
    this->view->getPage()->fireElementChanged(this->activeGeometryElement);
    this->view->getNoteView()->repaintSelection();
    return true;
}

auto EditSelection::removeSelectedGeometryConstraints() -> bool {
    if (!this->activeGeometryElement) {
        return false;
    }
    if (this->activeGeometryVertices.empty() && this->activeGeometryEdges.empty()) {
        return false;
    }

    const auto before = this->activeGeometryElement->geometry();
    auto after = before;
    std::vector<vn::geom::ConstraintId> removedIds;
    for (const auto& constraint: before.constraints()) {
        if (SelectionFactory::intersectsSelection(this->activeGeometryVertices, this->activeGeometryEdges, constraint)) {
            removedIds.push_back(constraint.id);
        }
    }

    if (removedIds.empty()) {
        return false;
    }

    for (const auto id: removedIds) {
        const bool removed = after.removeConstraint(id);
        xoj_assert(removed);
        (void) removed;
    }

    this->activeGeometryElement->replaceGeometry(after);
    this->undo->addUndoAction(std::make_unique<GeometryTopologyUndoAction>(
            this->sourcePage, this->activeGeometryElement, before, after,
            removedIds.size() == 1U ? _("Delete geometry constraint") : _("Delete geometry constraints")));
    this->contents->invalidateViewBuffer();
    this->view->getPage()->fireElementChanged(this->activeGeometryElement);
    this->view->getNoteView()->repaintSelection();
    return true;
}

void EditSelection::setEdgePan(bool pan) {
    if (pan && !this->edgePanHandler) {
        this->edgePanHandler =
                g_timeout_add(1000 / PAN_TIMER_RATE, vn::util::wrap_v<EditSelection::handleEdgePan>, this);
    } else if (!pan) {
        this->edgePanHandler.cancel();
        this->edgePanInhibitNext = false;
    }
}

bool EditSelection::isEdgePanning() const { return this->edgePanHandler; }

bool EditSelection::handleEdgePan(EditSelection* self) {
    if (self->view->getNoteView()->getControl()->getZoomControl()->isZoomPresentationMode()) {
        self->edgePanHandler.consume();
        self->edgePanInhibitNext = false;
        return false;
    }


    Layout* layout = self->view->getNoteView()->getLayout();
    const Settings* const settings = self->getView()->getNoteView()->getControl()->getSettings();
    const double zoom = self->view->getNoteView()->getZoom();

    // Helper function to compute scroll amount for a single dimension, based on visible region and selection bbox
    const auto computeScrollAmt = [&](double visMin, double visLen, double bboxMin, double bboxLen, double layoutSize,
                                      double relMousePos) -> double {
        const bool belowMin = bboxMin < visMin;
        const bool aboveMax = bboxMin + bboxLen > visMin + visLen;
        const double visMax = visMin + visLen;
        const double bboxMax = bboxMin + bboxLen;

        const bool isLargeSelection = bboxLen > visLen;
        const auto centerVis = (visMin + visLen / 2);
        const auto mouseDiff = (bboxMin + relMousePos * zoom - centerVis);

        // Scroll amount multiplier
        double mult = 0.0;

        const double maxMult = settings->getEdgePanMaxMult();
        int panDir = 0;

        // If the selection is larger than the view, scroll based on mouse position relative to the center of the
        // visible view Otherwise calculate bonus scroll amount due to proportion of selection out of view.
        if (isLargeSelection) {
            mult = maxMult * std::abs(mouseDiff) / (visLen);
            if (mouseDiff > 0.1 * visLen / 2.0) {
                panDir = 1;
            } else if (mouseDiff < -0.1 * visLen / 2.0) {
                panDir = -1;
            }
        } else {
            if (aboveMax) {
                panDir = 1;
                mult = maxMult * std::min(bboxLen, bboxMax - visMax) / bboxLen;
            } else if (belowMin) {
                panDir = -1;
                mult = maxMult * std::min(bboxLen, visMin - bboxMin) / bboxLen;
            }
        }

        // Base amount to translate selection (in document coordinates) per timer tick
        const double panSpeed = settings->getEdgePanSpeed();
        const double translateAmt = visLen * panSpeed / (100.0 * PAN_TIMER_RATE);

        // Amount to scroll the visible area by (in layout coordinates), accounting for multiplier
        double layoutScroll = zoom * panDir * (translateAmt * mult);

        // If scrolling past layout boundaries, clamp scroll amount to boundary
        if (visMin + layoutScroll < 0) {
            layoutScroll = -visMin;
        } else if (visMax + layoutScroll > layoutSize) {
            layoutScroll = std::max(0.0, layoutSize - visMax);
        }

        return layoutScroll;
    };
    // Compute scroll (for layout) and translation (for selection) for x and y
    const int layoutWidth = layout->getTotalPixelWidth();
    const int layoutHeight = layout->getTotalPixelHeight();
    const auto visRect = layout->getVisibleRect();
    const auto bbox = self->getBoundingBoxInView();
    const auto layoutScrollX =
            computeScrollAmt(visRect.x, visRect.width, bbox.x, bbox.width, layoutWidth, self->relMousePosX);
    const auto layoutScrollY =
            computeScrollAmt(visRect.y, visRect.height, bbox.y, bbox.height, layoutHeight, self->relMousePosY);
    const auto translateX = layoutScrollX / zoom;
    const auto translateY = layoutScrollY / zoom;

    // Perform the scrolling
    if (self->isMoving() && (layoutScrollX != 0.0 || layoutScrollY != 0.0)) {
        layout->scrollRelative(layoutScrollX, layoutScrollY);  // May create a page
        self->moveSelection(translateX, translateY);

        if (PageView* v = self->getPageViewUnderCursor(); v && v != self->view) {
            VertexNoteView* noteView = self->view->getNoteView();
            noteView->pageSelected(noteView->getControl()->getDocument()->indexOf(v->getPage()));

            self->translateToView(v);
        }

        // To prevent the selection from jumping and to reduce jitter, block the selection movement triggered by user
        // input
        self->edgePanInhibitNext = true;

        return true;
    } else {
        // No panning, so disable the timer.
        self->edgePanHandler.consume();
        self->edgePanInhibitNext = false;

        return false;
    }
}

auto EditSelection::getBoundingBoxInView() const -> Rectangle<double> {
    auto viewpos = this->view->getPixelPosition();
    double zoom = this->view->getNoteView()->getZoom();

    double sin = std::sin(this->rotation);
    double cos = std::cos(this->rotation);
    double w = std::abs(this->width * cos) + std::abs(this->height * sin);
    double h = std::abs(this->width * sin) + std::abs(this->height * cos);
    double cx = this->x + this->width / 2.0;
    double cy = this->y + this->height / 2.0;
    double minx = cx - w / 2.0;
    double miny = cy - h / 2.0;

    return {viewpos.x + minx * zoom, viewpos.y + miny * zoom, w * zoom, h * zoom};
}

void EditSelection::ensureWithinVisibleArea() {
    const Rectangle<double> viewRect = this->getBoundingBoxInView();
    // need to modify this to take into account the position
    // of the object, plus typecast because PageView takes ints
    this->view->getNoteView()->ensureRectIsVisible(static_cast<int>(viewRect.x), static_cast<int>(viewRect.y),
                                                  static_cast<int>(viewRect.width), static_cast<int>(viewRect.height));
}

/**
 * Get the cursor type for the current position (if 0 then the default cursor should be used)
 */
auto EditSelection::getSelectionTypeForPos(double x, double y, double zoom) -> CursorSelectionType {
    double x1 = getXOnView() * zoom;
    double x2 = x1 + this->width * zoom;
    double y1 = getYOnView() * zoom;
    double y2 = y1 + this->height * zoom;
    double xmin = std::min(x1, x2);
    double xmax = std::max(x1, x2);
    double ymin = std::min(y1, y2);
    double ymax = std::max(y1, y2);

    cairo_matrix_transform_point(&this->cmatrix, &x, &y);

    if (selectGeometryVertexHandleAt(x, y, zoom)) {
        this->hoveredGeometryElement = nullptr;
        this->hoveredGeometryEdge = vn::geom::InvalidEdgeId;
        return CURSOR_SELECTION_GEOMETRY_VERTEX;
    }
    this->hoveredGeometryVertexElement = nullptr;
    this->hoveredGeometryVertex = vn::geom::InvalidVertexId;
    if (selectGeometryEdgeAt(x, y, zoom)) {
        return CURSOR_SELECTION_GEOMETRY_EDGE;
    }
    this->hoveredGeometryVertexElement = nullptr;
    this->hoveredGeometryVertex = vn::geom::InvalidVertexId;
    this->hoveredGeometryElement = nullptr;
    this->hoveredGeometryEdge = vn::geom::InvalidEdgeId;

    const int EDGE_PADDING = (this->btnWidth / 2) + 2;
    const int BORDER_PADDING = (this->btnWidth / 2);

    if (x1 - EDGE_PADDING <= x && x <= x1 + EDGE_PADDING && y1 - EDGE_PADDING <= y && y <= y1 + EDGE_PADDING) {
        return CURSOR_SELECTION_TOP_LEFT;
    }

    if (x2 - EDGE_PADDING <= x && x <= x2 + EDGE_PADDING && y1 - EDGE_PADDING <= y && y <= y1 + EDGE_PADDING) {
        return CURSOR_SELECTION_TOP_RIGHT;
    }

    if (x1 - EDGE_PADDING <= x && x <= x1 + EDGE_PADDING && y2 - EDGE_PADDING <= y && y <= y2 + EDGE_PADDING) {
        return CURSOR_SELECTION_BOTTOM_LEFT;
    }

    if (x2 - EDGE_PADDING <= x && x <= x2 + EDGE_PADDING && y2 - EDGE_PADDING <= y && y <= y2 + EDGE_PADDING) {
        return CURSOR_SELECTION_BOTTOM_RIGHT;
    }

    if (xmin - (DELETE_PADDING + this->btnWidth) - BORDER_PADDING <= x &&
        x <= xmin - (DELETE_PADDING + this->btnWidth) + BORDER_PADDING && y1 - BORDER_PADDING <= y &&
        y <= y1 + BORDER_PADDING) {
        return CURSOR_SELECTION_DELETE;
    }


    if (supportRotation && xmax - BORDER_PADDING + ROTATE_PADDING + this->btnWidth <= x &&
        x <= xmax + BORDER_PADDING + ROTATE_PADDING + this->btnWidth && (y2 + y1) / 2 - 4 - BORDER_PADDING <= y &&
        (y2 + y1) / 2 + 4 + BORDER_PADDING >= y) {
        return CURSOR_SELECTION_ROTATE;
    }

    if (!this->preserveAspectRatio) {
        if (xmin <= x && x <= xmax) {
            if (y1 - BORDER_PADDING <= y && y <= y1 + BORDER_PADDING) {
                return CURSOR_SELECTION_TOP;
            }

            if (y2 - BORDER_PADDING <= y && y <= y2 + BORDER_PADDING) {
                return CURSOR_SELECTION_BOTTOM;
            }
        }

        if (ymin <= y && y <= ymax) {
            if (x1 - BORDER_PADDING <= x && x <= x1 + BORDER_PADDING) {
                return CURSOR_SELECTION_LEFT;
            }

            if (x2 - BORDER_PADDING <= x && x <= x2 + BORDER_PADDING) {
                return CURSOR_SELECTION_RIGHT;
            }
        }
    }

    if (xmin <= x && x <= xmax && ymin <= y && y <= ymax) {
        return CURSOR_SELECTION_MOVE;
    }

    return CURSOR_SELECTION_NONE;
}

/**
 * Paints the selection to cr, with the given zoom factor. The coordinates of cr
 * should be relative to the provided view by getView() (use translateEvent())
 */
void EditSelection::paint(cairo_t* cr, double zoom) {
    double x = this->x;
    double y = this->y;
    cairo_matrix_t baseMatrix{};
    cairo_get_matrix(cr, &baseMatrix);


    if (std::abs(this->rotation) > std::numeric_limits<double>::epsilon()) {
        this->rotation = snappingHandler.snapAngle(this->rotation, false);


        double rx = (snappedBounds.x + snappedBounds.width / 2) * zoom;
        double ry = (snappedBounds.y + snappedBounds.height / 2) * zoom;

        cairo_translate(cr, rx, ry);
        cairo_rotate(cr, this->rotation);

        // Draw the rotation point for debugging
        // cairo_set_source_rgb(cr, 0, 1, 0);
        // cairo_rectangle(cr, 0, 0, 10, 10);
        // cairo_stroke(cr);

        cairo_translate(cr, -rx, -ry);
    }
    this->contents->paint(cr, x, y, this->rotation, this->width, this->height, zoom);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    GdkRGBA selectionColor = view->getSelectionColor();

    // set the line always the same size on display
    cairo_set_line_width(cr, 1);

    const std::vector<double> dashes = {10.0, 10.0};
    Util::cairo_set_dash_from_vector(cr, dashes, 0);
    gdk_cairo_set_source_rgba(cr, &selectionColor);

    cairo_rectangle(cr, std::min(x, x + width) * zoom, std::min(y, y + height) * zoom, std::abs(width) * zoom,
                    std::abs(height) * zoom);

    // for debugging
    // cairo_rectangle(cr, snappedBounds.x * zoom, snappedBounds.y * zoom, snappedBounds.width * zoom,
    // snappedBounds.height * zoom);

    cairo_stroke_preserve(cr);
    auto applied = GdkRGBA{selectionColor.red, selectionColor.green, selectionColor.blue, 0.3};
    gdk_cairo_set_source_rgba(cr, &applied);
    cairo_fill(cr);

    ToolHandler* toolHandler = view->getNoteView()->getControl()->getToolHandler();
    if (toolHandler->getToolType() != TOOL_HAND) {
        cairo_set_dash(cr, nullptr, 0, 0);
        if (!this->preserveAspectRatio) {
            // top
            drawAnchorRect(cr, x + width / 2, y, zoom);
            // bottom
            drawAnchorRect(cr, x + width / 2, y + height, zoom);
            // left
            drawAnchorRect(cr, x, y + height / 2, zoom);
            // right
            drawAnchorRect(cr, x + width, y + height / 2, zoom);

            if (supportRotation) {
                // rotation handle
                drawAnchorRotation(cr,
                                   std::min(x, x + width) + std::abs(width) + (ROTATE_PADDING + this->btnWidth) / zoom,
                                   y + height / 2, zoom);
            }
        }

        // top left
        drawAnchorRect(cr, x, y, zoom);
        // top right
        drawAnchorRect(cr, x + width, y, zoom);
        // bottom left
        drawAnchorRect(cr, x, y + height, zoom);
        // bottom right
        drawAnchorRect(cr, x + width, y + height, zoom);

        drawDeleteRect(cr, std::min(x, x + width) - (DELETE_PADDING + this->btnWidth) / zoom, y, zoom);
    }

    drawGeometryEdgeHighlight(cr, x, y, zoom);
    drawGeometryConstraintBadges(cr, x, y, zoom);
    drawGeometryVertexHandles(cr, x, y, zoom);
    drawGeometrySnapIndicator(cr, zoom, baseMatrix);
}

void EditSelection::drawAnchorRotation(cairo_t* cr, double x, double y, double zoom) {
    GdkRGBA selectionColor = view->getSelectionColor();
    gdk_cairo_set_source_rgba(cr, &selectionColor);
    cairo_rectangle(cr, x * zoom - (this->btnWidth / 2), y * zoom - (this->btnWidth / 2), this->btnWidth,
                    this->btnWidth);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_fill(cr);
}

/**
 * draws an idicator where you can scale the selection
 */
void EditSelection::drawAnchorRect(cairo_t* cr, double x, double y, double zoom) {
    GdkRGBA selectionColor = view->getSelectionColor();
    gdk_cairo_set_source_rgba(cr, &selectionColor);
    cairo_rectangle(cr, x * zoom - (this->btnWidth / 2), y * zoom - (this->btnWidth / 2), this->btnWidth,
                    this->btnWidth);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_fill(cr);
}

void EditSelection::drawGeometryVertexHandles(cairo_t* cr, double x, double y, double zoom) const {
    const auto original = this->contents->getOriginalBounds();
    if (original.width == 0.0 && original.height == 0.0) {
        return;
    }

    const bool hasWidth = original.width != 0.0;
    const bool hasHeight = original.height != 0.0;
    const double fx = hasWidth ? this->width / original.width : 0.0;
    const double fy = hasHeight ? this->height / original.height : 0.0;

    for (const auto* element: this->contents->getElementsView()) {
        if (element->getType() != ELEMENT_GEOMETRY) {
            continue;
        }

        const auto* geometry = dynamic_cast<const vn::geom::GeometryElement*>(element);
        if (!geometry) {
            continue;
        }

        for (const auto& vertex: geometry->geometry().vertices()) {
            const double handleX = hasWidth ? x + (vertex.position.x - original.x) * fx : x + this->width / 2.0;
            const double handleY = hasHeight ? y + (vertex.position.y - original.y) * fy : y + this->height / 2.0;
            const bool selected = isGeometryVertexSelected(geometry, vertex.id);
            const bool hovered =
                    geometry == this->hoveredGeometryVertexElement && vertex.id == this->hoveredGeometryVertex;
            drawGeometryVertexHandle(cr, handleX, handleY, zoom, selected, hovered);
        }
    }
}

void EditSelection::drawGeometryEdgeHighlight(cairo_t* cr, double x, double y, double zoom) const {
    const auto* highlightedElement = this->hoveredGeometryElement ? this->hoveredGeometryElement : this->activeGeometryElement;
    if (!highlightedElement) {
        return;
    }

    const auto original = this->contents->getOriginalBounds();
    if (original.width == 0.0 && original.height == 0.0) {
        return;
    }

    const bool hasWidth = original.width != 0.0;
    const bool hasHeight = original.height != 0.0;
    const double fx = hasWidth ? this->width / original.width : 0.0;
    const double fy = hasHeight ? this->height / original.height : 0.0;

    GdkRGBA selectionColor = view->getSelectionColor();
    cairo_save(cr);
    cairo_set_dash(cr, nullptr, 0, 0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    const auto& geometry = highlightedElement->geometry();

    auto drawEdge = [&](const vn::geom::Edge& edge, double alpha, double widthScale) {
        const auto* start = geometry.vertex(edge.start);
        const auto* end = geometry.vertex(edge.end);
        if (!start || !end) {
            return;
        }

        const double startX = hasWidth ? x + (start->position.x - original.x) * fx : x + this->width / 2.0;
        const double startY = hasHeight ? y + (start->position.y - original.y) * fy : y + this->height / 2.0;
        const double endX = hasWidth ? x + (end->position.x - original.x) * fx : x + this->width / 2.0;
        const double endY = hasHeight ? y + (end->position.y - original.y) * fy : y + this->height / 2.0;

        GdkRGBA color = selectionColor;
        color.alpha *= alpha;
        cairo_set_line_width(cr, std::max(2.0, static_cast<double>(this->btnWidth) * widthScale));
        gdk_cairo_set_source_rgba(cr, &color);
        cairo_move_to(cr, startX * zoom, startY * zoom);
        cairo_line_to(cr, endX * zoom, endY * zoom);
        cairo_stroke(cr);
    };

    for (const auto& edge: geometry.edges()) {
        if (edge.kind != vn::geom::EdgeKind::Line) {
            continue;
        }

        const bool edgeHovered =
                highlightedElement == this->hoveredGeometryElement && edge.id == this->hoveredGeometryEdge;
        const bool edgeSelected = isGeometryEdgeSelected(highlightedElement, edge.id);
        drawEdge(edge, edgeHovered ? 1.0 : edgeSelected ? 0.85 : 0.45,
                 edgeHovered ? 0.5 : edgeSelected ? 0.4 : 0.28);
    }
    cairo_restore(cr);
}

void EditSelection::drawGeometryConstraintBadges(cairo_t* cr, double x, double y, double zoom) const {
    const auto constraints = selectedGeometryConstraints();
    if (constraints.empty()) {
        return;
    }

    constexpr double paddingX = 8.0;
    constexpr double paddingY = 5.0;
    constexpr double gap = 6.0;
    constexpr double fontSize = 11.0;

    cairo_save(cr);
    cairo_set_dash(cr, nullptr, 0, 0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, fontSize);

    GdkRGBA selectionColor = view->getSelectionColor();
    double badgeX = std::min(x, x + width) * zoom;
    double badgeY = (std::min(y, y + height) * zoom) - (fontSize + 18.0);

    for (const auto& constraint: constraints) {
        const auto label = SelectionFactory::constraintBadgeText(constraint);
        cairo_text_extents_t extents{};
        cairo_text_extents(cr, label.c_str(), &extents);

        const double badgeWidth = extents.width + paddingX * 2.0;
        const double badgeHeight = fontSize + paddingY * 2.0;

        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.96);
        cairo_rectangle(cr, badgeX, badgeY, badgeWidth, badgeHeight);
        cairo_fill_preserve(cr);
        gdk_cairo_set_source_rgba(cr, &selectionColor);
        cairo_set_line_width(cr, 1.25);
        cairo_stroke(cr);

        cairo_move_to(cr, badgeX + paddingX - extents.x_bearing,
                      badgeY + paddingY + fontSize - extents.y_bearing * 0.15);
        gdk_cairo_set_source_rgba(cr, &selectionColor);
        cairo_show_text(cr, label.c_str());

        badgeX += badgeWidth + gap;
    }

    cairo_restore(cr);
}

void EditSelection::drawGeometryVertexHandle(cairo_t* cr, double x, double y, double zoom, bool selected,
                                             bool hovered) const {
    GdkRGBA selectionColor = view->getSelectionColor();
    const double baseSize = std::max(5.0, static_cast<double>(this->btnWidth) * 0.65);
    const double size = hovered ? baseSize * 1.3 : selected ? baseSize * 1.2 : baseSize;
    const double px = x * zoom;
    const double py = y * zoom;

    cairo_save(cr);
    cairo_set_dash(cr, nullptr, 0, 0);
    cairo_set_line_width(cr, hovered ? 2.2 : selected ? 2.0 : 1.5);
    cairo_rectangle(cr, px - size / 2.0, py - size / 2.0, size, size);
    if (selected) {
        gdk_cairo_set_source_rgba(cr, &selectionColor);
    } else {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, hovered ? 0.98 : 0.95);
    }
    cairo_fill_preserve(cr);
    gdk_cairo_set_source_rgba(cr, &selectionColor);
    cairo_stroke(cr);

    if (selected || hovered) {
        cairo_arc(cr, px, py, size * 0.22, 0.0, 2.0 * M_PI);
        if (selected && !hovered) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.98);
        } else {
            gdk_cairo_set_source_rgba(cr, &selectionColor);
        }
        cairo_fill(cr);
    }
    cairo_restore(cr);
}

void EditSelection::drawGeometrySnapIndicator(cairo_t* cr, double zoom, const cairo_matrix_t& baseMatrix) const {
    if (this->mouseDownType != CURSOR_SELECTION_GEOMETRY_VERTEX || !this->activeGeometrySnapKind) {
        return;
    }

    double indicatorX = this->activeGeometrySnapPoint.x * zoom;
    double indicatorY = this->activeGeometrySnapPoint.y * zoom;
    cairo_matrix_transform_point(&this->cmatrix, &indicatorX, &indicatorY);

    cairo_save(cr);
    cairo_set_matrix(cr, &baseMatrix);
    vn::view::drawSnapIndicator(cr, Point(indicatorX, indicatorY), this->activeGeometrySnapKind);
    cairo_restore(cr);
}

bool EditSelection::selectGeometryVertexHandleAt(double x, double y, double zoom) {
    const auto original = this->contents->getOriginalBounds();
    if (original.width == 0.0 && original.height == 0.0) {
        return false;
    }

    const bool hasWidth = original.width != 0.0;
    const bool hasHeight = original.height != 0.0;
    const double fx = hasWidth ? this->width / original.width : 0.0;
    const double fy = hasHeight ? this->height / original.height : 0.0;
    const double hitRadius = std::max(6.0, static_cast<double>(this->btnWidth));

    bool selected = false;
    this->contents->forEachMutableElement([&](Element* element) {
        if (selected) {
            return;
        }

        if (element->getType() != ELEMENT_GEOMETRY) {
            return;
        }

        auto* geometry = dynamic_cast<vn::geom::GeometryElement*>(element);
        if (!geometry) {
            return;
        }

        for (const auto& vertex: geometry->geometry().vertices()) {
            const double modelX =
                    hasWidth ? this->x + (vertex.position.x - original.x) * fx : this->x + this->width / 2.0;
            const double modelY =
                    hasHeight ? this->y + (vertex.position.y - original.y) * fy : this->y + this->height / 2.0;
            const double handleX = modelX * zoom;
            const double handleY = modelY * zoom;
            if (std::hypot(x - handleX, y - handleY) <= hitRadius) {
                this->hoveredGeometryVertexElement = geometry;
                this->hoveredGeometryVertex = vertex.id;
                this->hoveredGeometryVertexPosition = vertex.position;
                selected = true;
                return;
            }
        }
    });

    return selected;
}

bool EditSelection::selectGeometryEdgeAt(double x, double y, double zoom) {
    this->hoveredGeometryElement = nullptr;
    this->hoveredGeometryEdge = vn::geom::InvalidEdgeId;

    const auto original = this->contents->getOriginalBounds();
    if (original.width == 0.0 && original.height == 0.0) {
        return false;
    }

    const bool hasWidth = original.width != 0.0;
    const bool hasHeight = original.height != 0.0;
    const double fx = hasWidth ? this->width / original.width : 0.0;
    const double fy = hasHeight ? this->height / original.height : 0.0;
    const double hitRadius = std::max(6.0, static_cast<double>(this->btnWidth));

    bool selected = false;
    this->contents->forEachMutableElement([&](Element* element) {
        if (selected || element->getType() != ELEMENT_GEOMETRY) {
            return;
        }

        auto* geometry = dynamic_cast<vn::geom::GeometryElement*>(element);
        if (!geometry) {
            return;
        }

        for (const auto& edge: geometry->geometry().edges()) {
            if (edge.kind != vn::geom::EdgeKind::Line) {
                continue;
            }

            const auto* start = geometry->geometry().vertex(edge.start);
            const auto* end = geometry->geometry().vertex(edge.end);
            if (!start || !end) {
                continue;
            }

            const double startX =
                    hasWidth ? this->x + (start->position.x - original.x) * fx : this->x + this->width / 2.0;
            const double startY =
                    hasHeight ? this->y + (start->position.y - original.y) * fy : this->y + this->height / 2.0;
            const double endX = hasWidth ? this->x + (end->position.x - original.x) * fx : this->x + this->width / 2.0;
            const double endY =
                    hasHeight ? this->y + (end->position.y - original.y) * fy : this->y + this->height / 2.0;

            const auto [distance, t] =
                    SelectionFactory::distanceToSegment(x, y, startX * zoom, startY * zoom, endX * zoom, endY * zoom);
            if (distance > hitRadius) {
                continue;
            }

            const double projectedX = startX + (endX - startX) * t;
            const double projectedY = startY + (endY - startY) * t;
            this->hoveredGeometryElement = geometry;
            this->hoveredGeometryEdge = edge.id;
            this->hoveredGeometryInsertPosition = geometryVertexPreviewToModel(projectedX, projectedY);
            selected = true;
            return;
        }
    });

    return selected;
}

auto EditSelection::geometryVertexPreviewToModel(double x, double y) const -> vn::geom::Vec2 {
    const auto original = this->contents->getOriginalBounds();
    const bool hasWidth = original.width != 0.0;
    const bool hasHeight = original.height != 0.0;
    const double fx = hasWidth ? this->width / original.width : 1.0;
    const double fy = hasHeight ? this->height / original.height : 1.0;

    return {hasWidth ? original.x + (x - this->x) / fx : original.x,
            hasHeight ? original.y + (y - this->y) / fy : original.y};
}

void EditSelection::clearGeometryVertexSelection() {
    clearGeometrySnapState();
    this->geometrySnapProvider.reset();
    this->geometrySnapEngine.clearProviders();
    this->activeGeometryBeforeDrag.reset();
    this->activeGeometryElement = nullptr;
    this->activeGeometryVertex = vn::geom::InvalidVertexId;
    this->activeGeometryVertexStart = {};
    this->activeGeometryVertexCurrent = {};
    this->activeGeometryVertices.clear();
    this->activeGeometryVertexStartPositions.clear();
    this->activeGeometryVertexCurrentPositions.clear();
    this->activeGeometryEdges.clear();
    this->activeGeometryVertexMoved = false;
}

void EditSelection::setSingleGeometryVertexSelection(vn::geom::GeometryElement* element, vn::geom::VertexId vertex,
                                                     vn::geom::Vec2 position) {
    this->activeGeometryElement = element;
    this->activeGeometryVertex = vertex;
    this->activeGeometryVertexStart = position;
    this->activeGeometryVertexCurrent = position;
    this->activeGeometryVertices = {vertex};
    this->activeGeometryVertexStartPositions = {position};
    this->activeGeometryVertexCurrentPositions = {position};
    this->activeGeometryEdges.clear();
    this->activeGeometryVertexMoved = false;
}

void EditSelection::toggleGeometryVertexSelection(vn::geom::GeometryElement* element, vn::geom::VertexId vertex,
                                                  vn::geom::Vec2 position) {
    if (element != this->activeGeometryElement || !this->activeGeometryEdges.empty()) {
        setSingleGeometryVertexSelection(element, vertex, position);
        return;
    }

    const auto index = findSelectedGeometryVertex(element, vertex);
    if (!index) {
        this->activeGeometryVertex = vertex;
        this->activeGeometryVertexStart = position;
        this->activeGeometryVertexCurrent = position;
        this->activeGeometryVertices.push_back(vertex);
        this->activeGeometryVertexStartPositions.push_back(position);
        this->activeGeometryVertexCurrentPositions.push_back(position);
        this->activeGeometryVertexMoved = false;
        return;
    }

    this->activeGeometryVertices.erase(this->activeGeometryVertices.begin() + static_cast<std::ptrdiff_t>(*index));
    this->activeGeometryVertexStartPositions.erase(this->activeGeometryVertexStartPositions.begin() +
                                                   static_cast<std::ptrdiff_t>(*index));
    this->activeGeometryVertexCurrentPositions.erase(this->activeGeometryVertexCurrentPositions.begin() +
                                                     static_cast<std::ptrdiff_t>(*index));

    if (this->activeGeometryVertices.empty()) {
        clearGeometryVertexSelection();
        return;
    }

    this->activeGeometryVertex = this->activeGeometryVertices.back();
    this->activeGeometryVertexStart = this->activeGeometryVertexStartPositions.back();
    this->activeGeometryVertexCurrent = this->activeGeometryVertexCurrentPositions.back();
    this->activeGeometryVertexMoved = false;
}

auto EditSelection::isGeometryVertexSelected(const vn::geom::GeometryElement* element, vn::geom::VertexId vertex) const
        -> bool {
    return element == this->activeGeometryElement && findSelectedGeometryVertex(this->activeGeometryElement, vertex).has_value();
}

auto EditSelection::findSelectedGeometryVertex(vn::geom::GeometryElement* element, vn::geom::VertexId vertex) const
        -> std::optional<std::size_t> {
    if (element != this->activeGeometryElement) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < this->activeGeometryVertices.size(); ++i) {
        if (this->activeGeometryVertices[i] == vertex) {
            return i;
        }
    }
    return std::nullopt;
}

void EditSelection::setSingleGeometryEdgeSelection(vn::geom::GeometryElement* element, vn::geom::EdgeId edge) {
    this->activeGeometryElement = element;
    this->activeGeometryVertex = vn::geom::InvalidVertexId;
    this->activeGeometryVertexStart = {};
    this->activeGeometryVertexCurrent = {};
    this->activeGeometryVertices.clear();
    this->activeGeometryVertexStartPositions.clear();
    this->activeGeometryVertexCurrentPositions.clear();
    this->activeGeometryEdges = {edge};
    this->activeGeometryBeforeDrag.reset();
    this->activeGeometryVertexMoved = false;
}

void EditSelection::toggleGeometryEdgeSelection(vn::geom::GeometryElement* element, vn::geom::EdgeId edge) {
    if (element != this->activeGeometryElement || !this->activeGeometryVertices.empty()) {
        setSingleGeometryEdgeSelection(element, edge);
        return;
    }

    const auto index = findSelectedGeometryEdge(element, edge);
    if (!index) {
        this->activeGeometryElement = element;
        this->activeGeometryEdges.push_back(edge);
        return;
    }

    this->activeGeometryEdges.erase(this->activeGeometryEdges.begin() + static_cast<std::ptrdiff_t>(*index));
    if (this->activeGeometryEdges.empty()) {
        clearGeometryVertexSelection();
    }
}

auto EditSelection::isGeometryEdgeSelected(const vn::geom::GeometryElement* element, vn::geom::EdgeId edge) const
        -> bool {
    return element == this->activeGeometryElement && findSelectedGeometryEdge(this->activeGeometryElement, edge).has_value();
}

auto EditSelection::findSelectedGeometryEdge(vn::geom::GeometryElement* element, vn::geom::EdgeId edge) const
        -> std::optional<std::size_t> {
    if (element != this->activeGeometryElement) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < this->activeGeometryEdges.size(); ++i) {
        if (this->activeGeometryEdges[i] == edge) {
            return i;
        }
    }
    return std::nullopt;
}

auto EditSelection::selectedGeometryConstraints() const -> std::vector<vn::geom::Constraint> {
    if (!this->activeGeometryElement) {
        return {};
    }

    std::vector<vn::geom::Constraint> constraints;
    for (const auto& constraint: this->activeGeometryElement->geometry().constraints()) {
        if (SelectionFactory::intersectsSelection(this->activeGeometryVertices, this->activeGeometryEdges,
                                                  constraint)) {
            constraints.push_back(constraint);
        }
    }
    return constraints;
}

/**
 * draws an idicator where you can delete the selection
 */
void EditSelection::drawDeleteRect(cairo_t* cr, double x, double y, double zoom) const {
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_rectangle(cr, x * zoom - (this->btnWidth / 2), y * zoom - (this->btnWidth / 2), this->btnWidth,
                    this->btnWidth);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_move_to(cr, x * zoom - (this->btnWidth / 2), y * zoom - (this->btnWidth / 2));
    cairo_rel_move_to(cr, this->btnWidth, 0);
    cairo_rel_line_to(cr, -this->btnWidth, this->btnWidth);
    cairo_rel_move_to(cr, this->btnWidth, 0);
    cairo_rel_line_to(cr, -this->btnWidth, -this->btnWidth);
    cairo_stroke(cr);
}


auto EditSelection::getView() -> PageView* { return this->view; }

void EditSelection::serialize(ObjectOutputStream& out) const {
    out.writeObject("EditSelection");

    out.writeDouble(this->x);
    out.writeDouble(this->y);
    out.writeDouble(this->width);
    out.writeDouble(this->height);

    out.writeDouble(this->snappedBounds.x);
    out.writeDouble(this->snappedBounds.y);
    out.writeDouble(this->snappedBounds.width);
    out.writeDouble(this->snappedBounds.height);

    out.writeDouble(this->rotation);

    this->contents->serialize(out);
    out.endObject();

    out.writeInt(static_cast<int>(this->getInsertionOrder().size()));
    for (const Element* e: this->getElementsView()) {
        e->serialize(out);
    }
}

void EditSelection::readSerialized(ObjectInputStream& in) {
    in.readObject("EditSelection");
    this->x = in.readDouble();
    this->y = in.readDouble();
    this->width = in.readDouble();
    this->height = in.readDouble();

    double xSnap = in.readDouble();
    double ySnap = in.readDouble();
    double wSnap = in.readDouble();
    double hSnap = in.readDouble();
    this->snappedBounds = Rectangle<double>{xSnap, ySnap, wSnap, hSnap};

    this->rotation = in.readDouble();

    this->contents =
            std::make_unique<EditSelectionContents>(vn::util::Rectangle<double>(), vn::util::Rectangle<double>(),
                                                    this->sourcePage, this->sourceLayer, this->view);
    this->contents->readSerialized(in);

    in.endObject();
}


