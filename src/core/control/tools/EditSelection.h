/*
 * VertexNote
 *
 * A selection for editing, every selection (Rect, Lasso...) is
 * converted to this one if the selection is finished
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <array>
#include <memory>  // for unique_ptr
#include <string>
#include <utility>  // for pair
#include <vector>   // for vector

#include <cairo.h>  // for cairo_t, cairo_matrix_t

#include "control/ToolEnums.h"               // for ToolSize
#include "model/Element.h"                   // for Element, Element::Index
#include "model/ElementContainer.h"          // for ElementContainer
#include "model/ElementInsertionPosition.h"  // for InsertionOrder
#include "model/PageRef.h"                   // for PageRef
#include "undo/UndoAction.h"                 // for UndoAction (ptr only)
#include "util/Color.h"                      // for Color
#include "util/PointerContainerView.h"       // for PointerContainerView
#include "util/Rectangle.h"                  // for Rectangle
#include "util/raii/GSourceURef.h"           // for GSourceURef
#include "util/serializing/Serializable.h"   // for Serializable
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/snapping/SnapEngine.h"

#include "CursorSelectionType.h"     // for CursorSelectionType, CURS...
#include "SnapToGridInputHandler.h"  // for SnapToGridInputHandler

class UndoRedoHandler;
class Layer;
class PageView;
class Selection;
class EditSelectionContents;
class DeleteUndoAction;
class LineStyle;
class ObjectInputStream;
class ObjectOutputStream;
class NoteFont;
class Document;
class EditSelection;
namespace vn::snap {
class GeometrySnapProvider;
}

namespace SelectionFactory {
auto createFromFloatingElement(Control* ctrl, const PageRef& page, Layer* layer, PageView* view, ElementPtr e)
        -> std::unique_ptr<EditSelection>;
auto createFromFloatingElements(Control* ctrl, const PageRef& page, Layer* layer, PageView* view,
                                InsertionOrder elts)  //
        -> std::pair<std::unique_ptr<EditSelection>, Range>;
auto createFromElementOnActiveLayer(Control* ctrl, const PageRef& page, PageView* view, const Element* e,
                                    Element::Index pos = Element::InvalidIndex)  //
        -> std::unique_ptr<EditSelection>;
auto createFromElementsOnActiveLayer(Control* ctrl, const PageRef& page, PageView* view, InsertionOrderRef elts)
        -> std::unique_ptr<EditSelection>;
/**
 * @brief Creates a new instance containing base->getElements() and *e. The content of *base is cleared but *base is not
 * destroyed.
 */
auto addElementFromActiveLayer(Control* ctrl, EditSelection* base, const Element* e, Element::Index pos)
        -> std::unique_ptr<EditSelection>;
/**
 * @brief Creates a new instance containing base->getElements() and the content of elts. The content of *base is cleared
 * but *base is not destroyed.
 */
auto addElementsFromActiveLayer(Control* ctrl, EditSelection* base, const InsertionOrderRef& elts)
        -> std::unique_ptr<EditSelection>;
};  // namespace SelectionFactory

class EditSelection: public ElementContainer, public Serializable {
public:
    EditSelection(Control* ctrl, InsertionOrder elts, const PageRef& page, Layer* layer, PageView* view,
                  const Range& bounds, const Range& snappingBounds);

    /// Construct an empty selection
    EditSelection(Control* ctrl, const PageRef& page, Layer* layer, PageView* view);

    ~EditSelection() override;

public:
    /**
     * get the X coordinate relative to the provided view (getView())
     * in document coordinates
     */
    double getXOnView() const;

    /**
     * Get the Y coordinate relative to the provided view (getView())
     * in document coordinates
     */
    double getYOnView() const;

    /**
     * @return The original X coordinates of the provided view in document
     * coordinates.
     */
    double getOriginalXOnView();

    /**
     * @return The original Y coordinates of the provided view in document
     * coordinates.
     */
    double getOriginalYOnView();

    /**
     * Get the width in document coordinates (multiple with zoom)
     */
    double getWidth() const;

    /**
     * Get the height in document coordinates (multiple with zoom)
     */
    double getHeight() const;

    /**
     * Get the bounding rectangle in document coordinates (multiple with zoom)
     */
    vn::util::Rectangle<double> getRect() const;

    /**
     * gets the minimal bounding box containing all elements of the selection used for e.g. grid snapping
     */
    vn::util::Rectangle<double> getSnappedBounds() const;

    /**
     * get the original bounding rectangle in document coordinates
     */
    vn::util::Rectangle<double> getOriginalBounds() const;

    /**
     * Get the rotation angle of the selection
     */
    double getRotation() const;

    /**
     * Get if the selection supports being rotated
     */
    bool isRotationSupported() const;

    /**
     * Get the source page (where the selection was done)
     */
    PageRef getSourcePage() const;

    /**
     * Get the source layer (form where the Elements come)
     */
    Layer* getSourceLayer() const;

    inline PageView* getView() const { return view; }

public:
    /**
     * Sets the tool size for pen or eraser, returns an undo action
     * (or nullptr if nothing is done)
     */
    UndoActionPtr setSize(ToolSize size, const double* thicknessPen, const double* thicknessHighlighter,
                          const double* thicknessEraser);

    /**
     * Set the line style of all strokes, return an undo action
     * (Or nullptr if nothing done)
     */
    UndoActionPtr setLineStyle(LineStyle style);

    /**
     * Set the color of all elements, return an undo action
     * (Or nullptr if nothing done, e.g. because there is only an image)
     */
    UndoActionPtr setColor(Color color);

    /**
     * Sets the font of all containing text elements, return an undo action
     * (or nullptr if there are no Text elements)
     */
    UndoActionPtr setFont(const NoteFont& font);

    /**
     * Fills the undo item if the selection is deleted
     * the selection is cleared after
     */
    void fillUndoItem(DeleteUndoAction* undo);

    /**
     * Fills the stroke, return an undo action
     * (Or nullptr if nothing done, e.g. because there is only an image)
     */
    UndoActionPtr setFill(int alphaPen, int alphaHighligther);

public:
    /**
     * Add an element to the this selection
     * @param pos: specifies the index of the element from the source layer,
     * in case we want to replace it back where it came from.
     */
    void addElement(ElementPtr e, Element::Index pos);

    /**
     * Returns all containing elements of this selection
     */
    auto getElementsView() const -> vn::util::PointerContainerView<std::vector<Element*>>;

    void forEachElement(std::function<void(const Element*)> f) const override;

    /**
     * Returns the insert order of this selection
     */
    auto getInsertionOrder() const -> InsertionOrder const&;

    enum class OrderChange {
        BringToFront,
        BringForward,
        SendBackward,
        SendToBack,
    };

    static constexpr std::array<std::string_view, 4> orderChangeNames{"bringToFront", "bringForward", "sendBackward",
                                                                      "sendToBack"};
    static constexpr std::array<OrderChange, 4> allChanges = {OrderChange::BringToFront, OrderChange::BringForward,
                                                              OrderChange::SendBackward, OrderChange::SendToBack};

    static constexpr auto orderChangeToString(const OrderChange change) -> std::string_view {
        return orderChangeNames.at(static_cast<size_t>(change));
    }

    /**
     * Change the insert order of this selection.
     */
    auto rearrangeInsertionOrder(const OrderChange change) -> UndoActionPtr;

    /**
     * Finish the current movement
     * (should be called in the mouse-button-released event handler)
     */
    void mouseUp();

    /**
     * Move the selection
     */
    void moveSelection(double dx, double dy, bool addMoveUndo = false);

    /**
     * Get the cursor type for the current position (if 0 then the default cursor should be used)
     */
    CursorSelectionType getSelectionTypeForPos(double x, double y, double zoom);

    /**
     * Paints the selection to cr, with the given zoom factor. The coordinates of cr
     * should be relative to the provided view by getView() (use translateEvent())
     */
    void paint(cairo_t* cr, double zoom);

    /**
     * Gets the selection's bounding box in view coordinates. This takes document zoom
     * and selection rotation into account.
     */
    auto getBoundingBoxInView() const -> vn::util::Rectangle<double>;

    /**
     * If the selection is outside the visible area correct the coordinates
     */
    void ensureWithinVisibleArea();

public:
    /**
     * Handles mouse input for moving and resizing, in pixel-coordinates relative to "view"
     */
    void mouseDown(CursorSelectionType type, double x, double y, bool shiftDown = false);

    /**
     * Handles mouse input for moving and resizing, in pixel-coordinates relative to "view"
     */
    void mouseMove(double x, double y, bool alt);

    /**
     * If the user is currently moving the selection.
     */
    bool isMoving() const;

    /**
     * If the user started pressing the delete button
     */
    bool isDeleting() const;

    void copySelection();
    [[nodiscard]] auto deleteActiveGeometryVertex() -> bool;
    [[nodiscard]] auto insertActiveGeometryVertexOnEdge() -> bool;
    [[nodiscard]] auto insertGeometryVertexAt(double x, double y, double zoom) -> bool;
    [[nodiscard]] auto applyGeometryConstraint(vn::geom::ConstraintKind kind) -> bool;
    [[nodiscard]] auto removeSelectedGeometryConstraints() -> bool;
    [[nodiscard]] auto selectedFixedLengthConstraint() const -> std::optional<vn::geom::Constraint>;
    [[nodiscard]] auto updateSelectedFixedLengthConstraint(double value) -> bool;

public:
    PageView* getView();

public:
    // Serialize interface
    void serialize(ObjectOutputStream& out) const override;
    void readSerialized(ObjectInputStream& in) override;


    /// Applies the transformation to the selected elements, empties the selection and return the modified elements
    InsertionOrder makeMoveEffective();

private:
    /**
     * Draws an indicator where you can scale the selection
     */
    void drawAnchorRect(cairo_t* cr, double x, double y, double zoom);

    /**
     * Draws an indicator where you can rotate the selection
     */
    void drawAnchorRotation(cairo_t* cr, double x, double y, double zoom);


    /**
     * Draws an indicator where you can delete the selection
     */
    void drawDeleteRect(cairo_t* cr, double x, double y, double zoom) const;

    /**
     * Draw VertexNote vertex handles for selected object-based geometry.
     */
    void drawGeometryEdgeHighlight(cairo_t* cr, double x, double y, double zoom) const;
    void drawGeometryConstraintBadges(cairo_t* cr, double x, double y, double zoom) const;
    void drawGeometryVertexHandles(cairo_t* cr, double x, double y, double zoom) const;
    void drawGeometryVertexHandle(cairo_t* cr, double x, double y, double zoom, bool selected, bool hovered) const;
    void drawGeometrySnapIndicator(cairo_t* cr, double zoom, const cairo_matrix_t& baseMatrix) const;
    bool selectGeometryVertexHandleAt(double x, double y, double zoom);
    bool selectGeometryEdgeAt(double x, double y, double zoom);
    [[nodiscard]] auto geometryVertexPreviewToModel(double x, double y) const -> vn::geom::Vec2;
    [[nodiscard]] auto snapGeometryVertexDragPosition(vn::geom::Vec2 modelPosition, bool alt, double zoom)
            -> vn::geom::Vec2;
    [[nodiscard]] auto applyGeometryConstraints(vn::geom::GeometryObject& object) const -> bool;
    void rebuildGeometrySnapEngine();
    void clearGeometrySnapState();
    void clearGeometryVertexSelection();
    void setSingleGeometryVertexSelection(vn::geom::GeometryElement* element, vn::geom::VertexId vertex,
                                          vn::geom::Vec2 position);
    void toggleGeometryVertexSelection(vn::geom::GeometryElement* element, vn::geom::VertexId vertex,
                                       vn::geom::Vec2 position);
    [[nodiscard]] auto isGeometryVertexSelected(const vn::geom::GeometryElement* element, vn::geom::VertexId vertex) const
            -> bool;
    [[nodiscard]] auto findSelectedGeometryVertex(vn::geom::GeometryElement* element, vn::geom::VertexId vertex) const
            -> std::optional<std::size_t>;
    void setSingleGeometryEdgeSelection(vn::geom::GeometryElement* element, vn::geom::EdgeId edge);
    void toggleGeometryEdgeSelection(vn::geom::GeometryElement* element, vn::geom::EdgeId edge);
    [[nodiscard]] auto isGeometryEdgeSelected(const vn::geom::GeometryElement* element, vn::geom::EdgeId edge) const
            -> bool;
    [[nodiscard]] auto findSelectedGeometryEdge(vn::geom::GeometryElement* element, vn::geom::EdgeId edge) const
            -> std::optional<std::size_t>;
    [[nodiscard]] auto selectedGeometryConstraints() const -> std::vector<vn::geom::Constraint>;


    /**
     * Finishes all pending changes, move the elements, scale the elements and add
     * them to new layer if any or to the old if no new layer
     */
    void finalizeSelection();

    /**
     * Gets the PageView under the cursor
     */
    PageView* getPageViewUnderCursor();

    /**
     * Translate all coordinates which are relative to the current view to the new view,
     * and set the attribute view to the new view
     */
    void translateToView(PageView* v);

    /**
     * Updates rotation matrix
     */
    void updateMatrix();
    void rebaseSelectionBounds();

    /**
     * scales and shifts to update bounding boxes
     */
    void scaleShift(double fx, double fy, bool changeLeft, bool changeTop);

    /**
     * Set edge panning signal.
     */
    void setEdgePan(bool edgePan);

    /**
     * Whether the edge pan signal is set.
     */
    bool isEdgePanning() const;

    static bool handleEdgePan(EditSelection* self);

private:  // DATA
    /**
     * The position (and rotation) relative to the current view
     */
    double x{};
    double y{};
    double rotation = 0;

    /**
     * Use to translate to rotated selection
     */
    cairo_matrix_t cmatrix{};

    /**
     * The size, including the padding and frame
     */
    double width{};
    double height{};

    /**
     * The size and dimensions for snapping
     */
    vn::util::Rectangle<double> snappedBounds{};


    /**
     * Mouse coordinates for moving / resizing
     */
    CursorSelectionType mouseDownType = CURSOR_SELECTION_NONE;
    double relMousePosX{};
    double relMousePosY{};
    double relMousePosRotX{};
    double relMousePosRotY{};

    vn::geom::GeometryElement* activeGeometryElement = nullptr;
    vn::geom::VertexId activeGeometryVertex = vn::geom::InvalidVertexId;
    vn::geom::Vec2 activeGeometryVertexStart;
    vn::geom::Vec2 activeGeometryVertexCurrent;
    std::vector<vn::geom::VertexId> activeGeometryVertices;
    std::vector<vn::geom::Vec2> activeGeometryVertexStartPositions;
    std::vector<vn::geom::Vec2> activeGeometryVertexCurrentPositions;
    std::vector<vn::geom::EdgeId> activeGeometryEdges;
    std::optional<vn::geom::GeometryObject> activeGeometryBeforeDrag;
    bool activeGeometryVertexMoved = false;
    vn::geom::GeometryElement* hoveredGeometryVertexElement = nullptr;
    vn::geom::VertexId hoveredGeometryVertex = vn::geom::InvalidVertexId;
    vn::geom::Vec2 hoveredGeometryVertexPosition;
    vn::geom::GeometryElement* hoveredGeometryElement = nullptr;
    vn::geom::EdgeId hoveredGeometryEdge = vn::geom::InvalidEdgeId;
    vn::geom::Vec2 hoveredGeometryInsertPosition;
    std::optional<vn::snap::SnapKind> activeGeometrySnapKind;
    vn::geom::Vec2 activeGeometrySnapPoint;
    std::shared_ptr<vn::snap::GeometrySnapProvider> geometrySnapProvider;
    vn::snap::SnapEngine geometrySnapEngine;

    /**
     * If both scale axes should have the same scale factor, e.g. for Text
     * (we can only set the font size for text)
     */
    bool preserveAspectRatio = false;

    /**
     * If mirrors are allowed e.g. for strokes
     */
    bool supportMirroring = true;

    /**
     * Support rotation
     */
    bool supportRotation = true;

    /**
     * Size of the editing handles
     */
    int btnWidth{8};

    /**
     * The source page (form where the Elements come)
     */
    PageRef sourcePage;

    /**
     * The source layer (form where the Elements come)
     */
    Layer* sourceLayer{};

    /**
     * The contents of the selection
     */
    std::unique_ptr<EditSelectionContents> contents;

private:  // HANDLER
    /**
     * The page view for the anchor
     */
    PageView* view{};

    /**
     * Undo redo handler
     */
    UndoRedoHandler* undo{};

    /**
     * The handler for snapping points
     */
    SnapToGridInputHandler snappingHandler;

    /**
     * Edge pan timer
     */
    vn::util::GSourceURef edgePanHandler;

    /**
     * Inhibit the next move event after edge panning finishes. This prevents
     * the selection from teleporting if the page has changed during panning.
     * Additionally, this reduces the amount of "jitter" resulting from moving
     * the selection in mouseDown while edge panning.
     */
    bool edgePanInhibitNext = false;
};
