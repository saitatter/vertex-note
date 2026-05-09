# VertexNote Architecture Notes

VertexNote is a long-term fork of Xournal++ focused on CAD-inspired technical note-taking.
The fork must preserve existing Xournal++ behavior while adding precise, object-based geometry
features incrementally.

## Current Xournal++-Derived Subsystems

- Input dispatch starts in `src/core/gui/PageView.cpp`, where `PageView` selects a tool handler.
- Freehand strokes are handled by `src/core/control/tools/StrokeHandler.*`.
- Drag-created shapes are handled by `src/core/control/tools/BaseShapeHandler.*` and subclasses.
- Click-based spline drawing already exists in `src/core/control/tools/SplineHandler.*`.
- The document model is `Document -> NotePage -> Layer -> Element`.
- Existing persistent element types are stroke, image, teximage, and text.
- Rendering dispatches through `src/core/view/ElementView.cpp`.
- Stroke rendering uses Cairo through `src/core/view/StrokeView.*`.
- File IO is implemented by `src/core/control/xojfile/SaveHandler.*`,
  `LoadHandler.*`, and `XmlParser.*`.
- Undo/redo is command-based under `src/core/undo`.

## Architectural Rule

Do not turn `Stroke` into a CAD object. Keep existing Xournal++ content stroke-based.
Add VertexNote geometry as object-based elements that can generate stroke-compatible fallback
representations for older `.xopp` readers.

## Stroke-Based Subsystems

These should remain stroke-based:

- Pen and highlighter input.
- Pressure-sensitive strokes.
- Whiteout strokes and eraser behavior.
- Existing drag line, rectangle, ellipse, arrow, and shape recognizer output.
- Existing `.xopp` documents.
- Export fallback paths.

## Object-Based Subsystems

These should become VertexNote geometry objects:

- Explicit vertices.
- Edges and polylines.
- Arcs, circles, and construction geometry.
- Editable endpoints and control points.
- Snapping targets and intersections.
- Persistent geometric constraints.
- Future projected 3D wireframes.

## Proposed Model

```cpp
namespace vn::geom {
using ObjectId = uint64_t;
using VertexId = uint64_t;
using EdgeId = uint64_t;
using ConstraintId = uint64_t;

struct Vec2 {
    double x;
    double y;
};

struct Vec3 {
    double x;
    double y;
    double z;
};

struct Vertex {
    VertexId id;
    Vec2 position;
    ObjectId owner;
    uint32_t flags;
};

enum class EdgeKind {
    Line,
    Arc,
    CubicBezier,
    ConstructionLine,
};

struct Edge {
    EdgeId id;
    EdgeKind kind;
    VertexId a;
    VertexId b;
    std::vector<VertexId> controls;
};

class GeometryElement final : public Element {
public:
    ObjectId objectId() const;
    std::unique_ptr<Stroke> makeXoppFallbackStroke() const;
};
}
```

## Snapping

Introduce a provider-based snap engine instead of adding more cases to `SnapToGridInputHandler`.
The current grid snapper should become one provider.

```cpp
enum class SnapKind {
    Grid,
    ExplicitVertex,
    EdgeEndpoint,
    Midpoint,
    EdgeProjection,
    Intersection,
    Tangent,
    Perpendicular,
    ConstraintGuide,
};

struct SnapCandidate {
    SnapKind kind;
    vn::geom::Vec2 pagePoint;
    double screenDistance;
    double priority;
    vn::geom::ObjectId object;
    vn::geom::VertexId vertex;
};

class ISnapProvider {
public:
    virtual void query(const SnapQuery& query, std::vector<SnapCandidate>& out) const = 0;
};

class SnapEngine {
public:
    SnapResult snap(const SnapQuery& query);
};
```

Use per-page spatial indexes for vertices and edge bounding boxes. Intersection snapping should be
computed lazily from nearby edge candidates and cached by page revision.

## Click-Based Tools

Use `SplineHandler` as the precedent, but create a separate base for VertexNote tools:

```cpp
class ClickDrawingHandler : public InputHandler {
protected:
    virtual void addVertex(SnapResult point) = 0;
    virtual void updatePreview(SnapResult hover) = 0;
    virtual std::unique_ptr<vn::geom::GeometryElement> finalizeObject() = 0;
};
```

Initial tools:

- `LineByClicksHandler`
- `PolylineHandler`
- `RectangleByVerticesHandler`
- `ArcBy3PointsHandler`
- `CircleByCenterRadiusHandler`

## Constraints

Constraints should be persistent model objects. Start with creation-time and edit-time behavior;
defer a full solver until the data model is stable.

```cpp
enum class ConstraintKind {
    Coincident,
    Horizontal,
    Vertical,
    Parallel,
    Perpendicular,
    EqualLength,
    FixedLength,
    FixedAngle,
    Radius,
    OnEdge,
};

struct Constraint {
    ConstraintId id;
    ConstraintKind kind;
    std::vector<VertexId> vertices;
    std::vector<EdgeId> edges;
    double value;
};
```

Solve constraints per connected component, not per document.

Initial implementation status:

- `GeometryObject` stores validated `Constraint` records with object-local IDs.
- Constraint solving is implemented for all 7 supported kinds.
- Edit handles and vertex drag with constraint enforcement are working.
- Undo/redo actions cover constraint creation, deletion, and geometry edits.

## Undo/Redo

Geometry insertion can initially reuse `InsertUndoAction` if `GeometryElement` inherits `Element`.
Geometry edits need dedicated undo actions:

- `MoveVertexUndoAction`
- `ModifyGeometryObjectUndoAction`
- `AddConstraintUndoAction`
- `RemoveConstraintUndoAction`
- `CompositeGeometryUndoAction`

Continuous drags should collapse into one undo action with old/new snapshots.

## File Compatibility

For compatibility with older Xournal++:

1. Save a normal stroke fallback for each geometry object.
2. Save VertexNote extension metadata that references the fallback by stable ID.
3. On load, VertexNote should prefer the geometry metadata and regenerate fallback strokes as needed.

Old Xournal++ versions can display the fallback strokes. They will not preserve VertexNote metadata
after saving, which should be documented as a compatibility limitation.

## Roadmap

### Phase 1: Vertex System ✅

- ✅ `GeometryElement` with IDs, vertices, edges.
- ✅ Fallback stroke generation.
- ✅ `GeometryElementView`.
- ✅ Save/load extension scaffolding.

### Phase 2: Snapping Engine ✅

- ✅ Provider-based `SnapEngine` chooses highest-priority candidate.
- ✅ `GridSnapProvider` covers regular grid candidates.
- ✅ `GeometrySnapProvider` covers vertices, midpoints, projections, intersections.
- ✅ `PageGeometryCollector` supplies visible page geometry at tool start.
- Per-page spatial index and intersection caching deferred until geometry
  elements scale beyond current workloads.

### Phase 3: Geometric Primitives ✅

- ✅ `LineByClicksHandler` creates two-vertex elements.
- ✅ `PolylineByClicksHandler` creates multi-edge elements from discrete clicks.
- ✅ `RectangleByVerticesHandler` creates four-edge elements from corner clicks.
- ✅ Arc (3-click), circle, construction line, and construction circle tools.
- ✅ Geometry snapping first, grid fallback.
- ✅ Process-local IDs via `GeometryIdGenerator`.
- Existing Xournal++ drag tools remain unchanged.

### Phase 4: Editable Constraints ✅

- ✅ Constraint objects: Coincident, Horizontal, Vertical, FixedLength, Radius,
  Parallel, Perpendicular.
- ✅ Constraint creation, deletion, and value editing.
- ✅ Connected-component solver.
- ✅ Vertex drag with constraint enforcement.
- ✅ Grouped undo actions for geometry edits.

### Phase 5: Lightweight 3D Wireframe Layer

- Add 3D vertices and edges.
- Add projection cache.
- Add camera/projection settings per object.
- Expose projected vertices and intersections to the snap engine.
