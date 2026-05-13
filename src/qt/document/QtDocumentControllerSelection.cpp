/*
 * VertexNote
 *
 * Qt document controller element selection helpers.
 */

#include "QtDocumentController.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "model/Element.h"
#include "model/Layer.h"
#include "model/NotePage.h"

namespace {

constexpr double GeometrySelectionEpsilon = 1e-9;
constexpr double Pi = 3.14159265358979323846;

auto pointInRect(const Point& point, double left, double right, double top, double bottom) -> bool {
    return point.x >= left && point.x <= right && point.y >= top && point.y <= bottom;
}

auto cross(const Point& a, const Point& b, const Point& c) -> double {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

auto pointOnSegment(const Point& point, const Point& a, const Point& b) -> bool {
    return std::abs(cross(a, b, point)) <= GeometrySelectionEpsilon &&
           point.x >= std::min(a.x, b.x) - GeometrySelectionEpsilon &&
           point.x <= std::max(a.x, b.x) + GeometrySelectionEpsilon &&
           point.y >= std::min(a.y, b.y) - GeometrySelectionEpsilon &&
           point.y <= std::max(a.y, b.y) + GeometrySelectionEpsilon;
}

auto segmentsIntersect(const Point& a, const Point& b, const Point& c, const Point& d) -> bool {
    const double abC = cross(a, b, c);
    const double abD = cross(a, b, d);
    const double cdA = cross(c, d, a);
    const double cdB = cross(c, d, b);

    if (((abC > GeometrySelectionEpsilon && abD < -GeometrySelectionEpsilon) ||
         (abC < -GeometrySelectionEpsilon && abD > GeometrySelectionEpsilon)) &&
        ((cdA > GeometrySelectionEpsilon && cdB < -GeometrySelectionEpsilon) ||
         (cdA < -GeometrySelectionEpsilon && cdB > GeometrySelectionEpsilon))) {
        return true;
    }

    return pointOnSegment(c, a, b) || pointOnSegment(d, a, b) || pointOnSegment(a, c, d) ||
           pointOnSegment(b, c, d);
}

auto rectEdges(double left, double right, double top, double bottom) -> std::array<std::pair<Point, Point>, 4> {
    const Point topLeft(left, top);
    const Point topRight(right, top);
    const Point bottomRight(right, bottom);
    const Point bottomLeft(left, bottom);
    return {{{topLeft, topRight}, {topRight, bottomRight}, {bottomRight, bottomLeft}, {bottomLeft, topLeft}}};
}

auto segmentIntersectsRect(const Point& a, const Point& b, double left, double right, double top, double bottom)
        -> bool {
    if (pointInRect(a, left, right, top, bottom) || pointInRect(b, left, right, top, bottom)) {
        return true;
    }
    for (const auto& [edgeStart, edgeEnd]: rectEdges(left, right, top, bottom)) {
        if (segmentsIntersect(a, b, edgeStart, edgeEnd)) {
            return true;
        }
    }
    return false;
}

auto infiniteLineIntersectsRect(const Point& a, const Point& b, double left, double right, double top, double bottom)
        -> bool {
    if (std::hypot(b.x - a.x, b.y - a.y) <= GeometrySelectionEpsilon) {
        return pointInRect(a, left, right, top, bottom);
    }

    bool hasPositive = false;
    bool hasNegative = false;
    for (const auto& corner: {Point(left, top), Point(right, top), Point(right, bottom), Point(left, bottom)}) {
        const double value = cross(a, b, corner);
        hasPositive = hasPositive || value > GeometrySelectionEpsilon;
        hasNegative = hasNegative || value < -GeometrySelectionEpsilon;
        if (std::abs(value) <= GeometrySelectionEpsilon) {
            return true;
        }
    }
    return hasPositive && hasNegative;
}

auto normalizeAngle(double angle) -> double {
    while (angle < 0.0) {
        angle += 2.0 * Pi;
    }
    while (angle >= 2.0 * Pi) {
        angle -= 2.0 * Pi;
    }
    return angle;
}

auto angleWithinSweep(double angle, double start, double end) -> bool {
    angle = normalizeAngle(angle);
    start = normalizeAngle(start);
    end = normalizeAngle(end);
    if (end < start) {
        end += 2.0 * Pi;
    }
    if (angle < start) {
        angle += 2.0 * Pi;
    }
    return angle >= start - GeometrySelectionEpsilon && angle <= end + GeometrySelectionEpsilon;
}

auto circleIntersectsRect(const Point& center, double radius, double left, double right, double top, double bottom)
        -> bool {
    const double closestX = std::clamp(center.x, left, right);
    const double closestY = std::clamp(center.y, top, bottom);
    const double minDistance = std::hypot(closestX - center.x, closestY - center.y);

    double maxDistance = 0.0;
    for (const auto& corner: {Point(left, top), Point(right, top), Point(right, bottom), Point(left, bottom)}) {
        maxDistance = std::max(maxDistance, std::hypot(corner.x - center.x, corner.y - center.y));
    }
    return minDistance <= radius + GeometrySelectionEpsilon && maxDistance + GeometrySelectionEpsilon >= radius;
}

auto arcIntersectsRect(const vn::view::render::GeometryEdgeRenderModel& edge, const Point& center, double radius,
                       double left, double right, double top, double bottom) -> bool {
    if (!circleIntersectsRect(center, radius, left, right, top, bottom)) {
        return false;
    }
    if (edge.closedLoop) {
        return true;
    }

    const double startAngle = std::atan2(edge.start.y - center.y, edge.start.x - center.x);
    const double endAngle = std::atan2(edge.end.y - center.y, edge.end.x - center.x);
    for (const auto& [edgeStart, edgeEnd]: rectEdges(left, right, top, bottom)) {
        const double dx = edgeEnd.x - edgeStart.x;
        const double dy = edgeEnd.y - edgeStart.y;
        const double fx = edgeStart.x - center.x;
        const double fy = edgeStart.y - center.y;
        const double a = dx * dx + dy * dy;
        const double b = 2.0 * (fx * dx + fy * dy);
        const double c = fx * fx + fy * fy - radius * radius;
        const double discriminant = b * b - 4.0 * a * c;
        if (discriminant < -GeometrySelectionEpsilon || a <= GeometrySelectionEpsilon) {
            continue;
        }
        const double root = std::sqrt(std::max(0.0, discriminant));
        for (const double t: {(-b - root) / (2.0 * a), (-b + root) / (2.0 * a)}) {
            if (t < -GeometrySelectionEpsilon || t > 1.0 + GeometrySelectionEpsilon) {
                continue;
            }
            const Point intersection(edgeStart.x + dx * t, edgeStart.y + dy * t);
            const double angle = std::atan2(intersection.y - center.y, intersection.x - center.x);
            if (angleWithinSweep(angle, startAngle, endAngle)) {
                return true;
            }
        }
    }
    return pointInRect(edge.start, left, right, top, bottom) || pointInRect(edge.end, left, right, top, bottom);
}

}  // namespace

auto QtDocumentController::hitTestElement(std::size_t pageIndex, double pageX, double pageY,
                                          double maxDistance) const -> const Element* {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }

    const Element* best = nullptr;
    double bestDist = maxDistance;

    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return nullptr;
    }

    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        for (const Element* element: layer->getElementsView()) {
            if (!element) {
                continue;
            }
            const double dist = element->distanceTo(pageX, pageY);
            if (dist < bestDist) {
                bestDist = dist;
                best = element;
            }
        }
    }
    return best;
}

void QtDocumentController::selectElementAt(std::size_t pageIndex, double pageX, double pageY, double maxDistance,
                                           bool additive) {
    const Element* hit = hitTestElement(pageIndex, pageX, pageY, maxDistance);
    if (!hit) {
        if (!additive) {
            clearElementSelection();
        }
        return;
    }

    if (additive && this->currentSelection && this->currentSelection->pageIndex == pageIndex) {
        auto& elems = this->currentSelection->elements;
        auto it = std::find(elems.begin(), elems.end(), hit);
        if (it != elems.end()) {
            elems.erase(it);
            if (elems.empty()) {
                this->currentSelection.reset();
            }
        } else {
            elems.push_back(hit);
        }
    } else {
        this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = {hit}};
    }
}

void QtDocumentController::selectElementsInRect(std::size_t pageIndex, double x, double y, double w, double h) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }

    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return;
    }

    std::vector<const Element*> hits;
    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        for (const Element* element: layer->getElementsView()) {
            if (!element) {
                continue;
            }
            if (element->intersectsArea(x, y, w, h)) {
                hits.push_back(element);
            }
        }
    }

    if (hits.empty()) {
        this->currentSelection.reset();
    } else {
        this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = std::move(hits)};
    }
}

auto QtDocumentController::selectGeometryVerticesInRect(std::size_t pageIndex, double x, double y, double w, double h,
                                                        bool additive) -> bool {
    if (pageIndex >= this->pageSnapshots.size()) {
        if (!additive) {
            setSelectedGeometry(std::nullopt);
        }
        return false;
    }

    const double left = std::min(x, x + w);
    const double right = std::max(x, x + w);
    const double top = std::min(y, y + h);
    const double bottom = std::max(y, y + h);

    std::vector<QtGeometryHit> hits;
    std::optional<vn::geom::ObjectId> targetObjectId;
    for (const auto& drawable: this->pageSnapshots[pageIndex].drawables) {
        const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
        if (!geometry) {
            continue;
        }
        for (const auto& vertex: geometry->vertices) {
            if (vertex.position.x < left || vertex.position.x > right || vertex.position.y < top ||
                vertex.position.y > bottom) {
                continue;
            }
            if (!targetObjectId) {
                targetObjectId = geometry->objectId;
            }
            if (*targetObjectId != geometry->objectId) {
                continue;
            }

            vn::view::render::GeometryHitResult hit;
            hit.type = vn::view::render::GeometryHitType::Vertex;
            hit.objectId = geometry->objectId;
            hit.vertexId = vertex.id;
            hit.point = vertex.position;
            hits.push_back(QtGeometryHit{.pageIndex = pageIndex, .hit = hit});
        }
    }

    if (hits.empty()) {
        if (!additive) {
            setSelectedGeometry(std::nullopt);
        }
        return false;
    }

    bool append = additive;
    for (auto& hit: hits) {
        setSelectedGeometry(std::move(hit), append);
        append = true;
    }
    clearElementSelection();
    return true;
}

auto QtDocumentController::selectGeometryEdgesInRect(std::size_t pageIndex, double x, double y, double w, double h,
                                                     bool additive) -> bool {
    if (pageIndex >= this->pageSnapshots.size()) {
        if (!additive) {
            setSelectedGeometry(std::nullopt);
        }
        return false;
    }

    const double left = std::min(x, x + w);
    const double right = std::max(x, x + w);
    const double top = std::min(y, y + h);
    const double bottom = std::max(y, y + h);
    const auto edgeTouchesRect = [&](const vn::view::render::GeometryEdgeRenderModel& edge) {
        if (pointInRect(edge.start, left, right, top, bottom) ||
            pointInRect(edge.end, left, right, top, bottom)) {
            return true;
        }
        for (const auto& control: edge.controls) {
            if (pointInRect(control, left, right, top, bottom)) {
                return true;
            }
        }
        if (edge.kind == vn::geom::EdgeKind::ConstructionLine) {
            return infiniteLineIntersectsRect(edge.start, edge.end, left, right, top, bottom);
        }
        if ((edge.kind == vn::geom::EdgeKind::Arc || edge.kind == vn::geom::EdgeKind::ConstructionCircle) &&
            !edge.controls.empty()) {
            const auto& center = edge.controls.front();
            const double radius = std::hypot(edge.start.x - center.x, edge.start.y - center.y);
            return radius > GeometrySelectionEpsilon && arcIntersectsRect(edge, center, radius, left, right, top, bottom);
        }
        return segmentIntersectsRect(edge.start, edge.end, left, right, top, bottom);
    };

    std::vector<QtGeometryHit> hits;
    std::optional<vn::geom::ObjectId> targetObjectId;
    for (const auto& drawable: this->pageSnapshots[pageIndex].drawables) {
        const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
        if (!geometry) {
            continue;
        }
        for (const auto& edge: geometry->edges) {
            if (!edgeTouchesRect(edge)) {
                continue;
            }
            if (!targetObjectId) {
                targetObjectId = geometry->objectId;
            }
            if (*targetObjectId != geometry->objectId) {
                continue;
            }

            vn::view::render::GeometryHitResult hit;
            hit.type = vn::view::render::GeometryHitType::Edge;
            hit.objectId = geometry->objectId;
            hit.edgeId = edge.id;
            hit.point = Point((edge.start.x + edge.end.x) / 2.0, (edge.start.y + edge.end.y) / 2.0);
            hits.push_back(QtGeometryHit{.pageIndex = pageIndex, .hit = hit});
        }
    }

    if (hits.empty()) {
        if (!additive) {
            setSelectedGeometry(std::nullopt);
        }
        return false;
    }

    bool append = additive;
    for (auto& hit: hits) {
        setSelectedGeometry(std::move(hit), append);
        append = true;
    }
    clearElementSelection();
    return true;
}

auto QtDocumentController::selectGeometryObjectInRect(std::size_t pageIndex, double x, double y, double w, double h)
        -> bool {
    if (pageIndex >= this->pageSnapshots.size()) {
        setSelectedGeometryObject(std::nullopt);
        return false;
    }

    const double left = std::min(x, x + w);
    const double right = std::max(x, x + w);
    const double top = std::min(y, y + h);
    const double bottom = std::max(y, y + h);

    for (const auto& drawable: this->pageSnapshots[pageIndex].drawables) {
        const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
        if (!geometry || geometry->vertices.empty()) {
            continue;
        }
        bool intersects = false;
        for (const auto& vertex: geometry->vertices) {
            if (vertex.position.x >= left && vertex.position.x <= right && vertex.position.y >= top &&
                vertex.position.y <= bottom) {
                intersects = true;
                break;
            }
        }
        if (!intersects) {
            continue;
        }

        vn::view::render::GeometryHitResult hit;
        hit.type = vn::view::render::GeometryHitType::Edge;
        hit.objectId = geometry->objectId;
        hit.point = geometry->vertices.front().position;
        setSelectedGeometryObject(QtGeometryHit{.pageIndex = pageIndex, .hit = hit});
        clearElementSelection();
        return true;
    }

    setSelectedGeometryObject(std::nullopt);
    return false;
}

void QtDocumentController::clearElementSelection() { this->currentSelection.reset(); }

auto QtDocumentController::elementSelection() const -> const std::optional<QtElementSelection>& {
    return this->currentSelection;
}

auto QtDocumentController::selectionBounds() const -> std::optional<QtSelectionBounds> {
    if (!this->currentSelection || this->currentSelection->elements.empty()) {
        return std::nullopt;
    }

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    bool hasElement = false;

    for (const auto* element: this->currentSelection->elements) {
        if (!element) {
            continue;
        }
        const auto bounds = element->boundingRect();
        minX = std::min(minX, bounds.x);
        minY = std::min(minY, bounds.y);
        maxX = std::max(maxX, bounds.x + bounds.width);
        maxY = std::max(maxY, bounds.y + bounds.height);
        hasElement = true;
    }

    if (!hasElement) {
        return std::nullopt;
    }
    return QtSelectionBounds{.x = minX, .y = minY, .width = maxX - minX, .height = maxY - minY};
}

auto QtDocumentController::isElementSelected(const Element* e) const -> bool {
    if (!this->currentSelection || !e) {
        return false;
    }
    const auto& elems = this->currentSelection->elements;
    return std::find(elems.begin(), elems.end(), e) != elems.end();
}

auto QtDocumentController::elementsForPluginScope(std::string_view scope, ElementType type,
                                                  std::size_t currentPageIndex) const
        -> std::vector<QtPluginElementRef> {
    std::vector<QtPluginElementRef> result;
    if (!this->document) {
        return result;
    }

    const auto appendLayer = [&](std::size_t pageIndex, std::size_t layerIndex, const Layer* layer) {
        if (!layer) {
            return;
        }
        for (const auto* element: layer->getElementsView()) {
            if (element && element->getType() == type) {
                result.push_back(QtPluginElementRef{.element = element, .pageIndex = pageIndex, .layerIndex = layerIndex});
            }
        }
    };

    if (scope == "selection") {
        if (!this->currentSelection) {
            return result;
        }
        for (const auto* element: this->currentSelection->elements) {
            if (element && element->getType() == type) {
                result.push_back(QtPluginElementRef{
                        .element = element,
                        .pageIndex = this->currentSelection->pageIndex,
                        .layerIndex = 0U,
                });
            }
        }
        return result;
    }

    if (scope == "layer") {
        if (currentPageIndex >= this->document->getPageCount()) {
            return result;
        }
        auto page = this->document->getPage(currentPageIndex);
        const auto layerIndex = page ? static_cast<std::size_t>(page->getSelectedLayerId()) : 0U;
        auto layer = page ? page->getSelectedLayer() : nullptr;
        appendLayer(currentPageIndex, layerIndex, layer);
        return result;
    }

    const auto appendPage = [&](std::size_t pageIndex) {
        auto page = this->document->getPage(pageIndex);
        if (!page) {
            return;
        }
        const auto& layers = page->getLayers();
        for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
            appendLayer(pageIndex, layerIndex, layers[layerIndex]);
        }
    };

    if (scope == "page") {
        if (currentPageIndex < this->document->getPageCount()) {
            appendPage(currentPageIndex);
        }
        return result;
    }

    if (scope == "all") {
        for (std::size_t pageIndex = 0; pageIndex < this->document->getPageCount(); ++pageIndex) {
            appendPage(pageIndex);
        }
    }
    return result;
}

auto QtDocumentController::selectElementsByPluginRefs(std::size_t pageIndex,
                                                      const std::vector<const Element*>& refs) -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        return false;
    }

    std::vector<const Element*> selected;
    selected.reserve(refs.size());
    for (const auto* candidate: refs) {
        if (candidate && layer->indexOf(candidate) != Element::InvalidIndex &&
            std::find(selected.begin(), selected.end(), candidate) == selected.end()) {
            selected.push_back(candidate);
        }
    }

    if (selected.empty()) {
        this->currentSelection.reset();
        return false;
    }
    this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = std::move(selected)};
    return true;
}

auto QtDocumentController::colorSelectedElements(Color color) -> bool {
    if (!this->document || !this->currentSelection || this->currentSelection->elements.empty() ||
        this->currentSelection->pageIndex >= this->document->getPageCount()) {
        return false;
    }

    bool changed = false;
    this->document->lock();
    auto page = this->document->getPage(this->currentSelection->pageIndex);
    if (page) {
        for (auto* layer: page->getLayers()) {
            if (!layer) {
                continue;
            }
            for (auto& element: layer->getElements()) {
                const auto* ptr = element.get();
                if (ptr && std::find(this->currentSelection->elements.begin(), this->currentSelection->elements.end(),
                                     ptr) != this->currentSelection->elements.end()) {
                    element->setColor(color);
                    changed = true;
                }
            }
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::deleteSelectedElements() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }

    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (!page) {
        this->document->unlock();
        return false;
    }

    InsertionOrder removedElements;
    for (const auto* elem: this->currentSelection->elements) {
        for (auto* layer: page->getLayers()) {
            if (!layer) {
                continue;
            }
            auto removed = layer->removeElement(elem);
            if (removed.e) {
                removedElements.push_back(std::move(removed));
                break;
            }
        }
    }
    this->document->unlock();

    if (removedElements.empty()) {
        return false;
    }

    std::vector<const Element*> ptrs;
    ptrs.reserve(removedElements.size());
    for (const auto& ip: removedElements) {
        ptrs.push_back(ip.e.get());
    }

    pushHistory(QtHistoryEntry{
            .data = QtDeleteHistoryEntry{.pageIndex = pageIndex,
                                         .removedElements = std::move(removedElements),
                                         .elementPtrs = std::move(ptrs),
                                         .text = "Delete selection"}});
    clearElementSelection();
    rebuildPageSnapshots();
    return true;
}

void QtDocumentController::selectAllElements(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }

    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return;
    }

    std::vector<const Element*> allElements;
    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        for (const Element* element: layer->getElementsView()) {
            if (element) {
                allElements.push_back(element);
            }
        }
    }

    if (allElements.empty()) {
        this->currentSelection.reset();
    } else {
        this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = std::move(allElements)};
    }
}

auto QtDocumentController::copySelectedElements() -> std::vector<ElementPtr> {
    std::vector<ElementPtr> clones;
    if (!this->currentSelection || this->currentSelection->elements.empty()) {
        return clones;
    }

    clones.reserve(this->currentSelection->elements.size());
    for (const auto* elem: this->currentSelection->elements) {
        if (elem) {
            clones.push_back(elem->clone());
        }
    }
    return clones;
}

auto QtDocumentController::cutSelectedElements() -> std::vector<ElementPtr> {
    auto clones = copySelectedElements();
    if (!clones.empty()) {
        (void)deleteSelectedElements();
    }
    return clones;
}

auto QtDocumentController::pasteElements(std::size_t pageIndex, std::vector<ElementPtr> elements, double offsetX,
                                         double offsetY) -> bool {
    if (elements.empty() || !this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return false;
    }

    std::vector<const Element*> pastedPtrs;
    pastedPtrs.reserve(elements.size());
    for (auto& elem: elements) {
        elem->move(offsetX, offsetY);
        pastedPtrs.push_back(elem.get());
        layer->addElement(std::move(elem));
    }
    this->document->unlock();

    this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = std::move(pastedPtrs)};
    rebuildPageSnapshots();
    return true;
}

namespace {
auto findElementLayer(NotePage* page, const Element* elem) -> Layer* {
    if (!page || !elem) {
        return nullptr;
    }
    for (auto* layer: page->getLayers()) {
        if (!layer) {
            continue;
        }
        for (const auto& e: layer->getElements()) {
            if (e.get() == elem) {
                return layer;
            }
        }
    }
    return nullptr;
}
}  // namespace

auto QtDocumentController::bringSelectionToFront() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }
    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    bool changed = false;
    for (const auto* elem: this->currentSelection->elements) {
        auto* layer = findElementLayer(page.get(), elem);
        if (!layer) {
            continue;
        }
        auto removed = layer->removeElement(elem);
        if (removed.e) {
            layer->addElement(std::move(removed.e));
            changed = true;
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::sendSelectionToBack() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }
    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    bool changed = false;
    Element::Index insertIdx = 0;
    for (const auto* elem: this->currentSelection->elements) {
        auto* layer = findElementLayer(page.get(), elem);
        if (!layer) {
            continue;
        }
        auto removed = layer->removeElement(elem);
        if (removed.e) {
            layer->insertElement(std::move(removed.e), insertIdx);
            ++insertIdx;
            changed = true;
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::bringSelectionForward() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }
    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    bool changed = false;
    for (const auto* elem: this->currentSelection->elements) {
        auto* layer = findElementLayer(page.get(), elem);
        if (!layer) {
            continue;
        }
        const auto idx = layer->indexOf(elem);
        if (idx == Element::InvalidIndex) {
            continue;
        }
        const auto maxIdx = static_cast<Element::Index>(layer->getElements().size()) - 1;
        if (idx < maxIdx) {
            auto removed = layer->removeElement(elem);
            if (removed.e) {
                layer->insertElement(std::move(removed.e), idx + 1);
                changed = true;
            }
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::sendSelectionBackward() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }
    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    bool changed = false;
    for (const auto* elem: this->currentSelection->elements) {
        auto* layer = findElementLayer(page.get(), elem);
        if (!layer) {
            continue;
        }
        const auto idx = layer->indexOf(elem);
        if (idx == Element::InvalidIndex) {
            continue;
        }
        if (idx > 0) {
            auto removed = layer->removeElement(elem);
            if (removed.e) {
                layer->insertElement(std::move(removed.e), idx - 1);
                changed = true;
            }
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::beginMoveSelection(double pageX, double pageY) -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty()) {
        return false;
    }

    this->moveState = QtMoveState{.startX = pageX,
                                  .startY = pageY,
                                  .currentDx = 0.0,
                                  .currentDy = 0.0,
                                  .elements = this->currentSelection->elements,
                                  .pageIndex = this->currentSelection->pageIndex};
    return true;
}

auto QtDocumentController::updateMoveSelection(double pageX, double pageY) -> bool {
    if (!this->moveState || !this->document) {
        return false;
    }

    const double newDx = pageX - this->moveState->startX;
    const double newDy = pageY - this->moveState->startY;
    const double deltaDx = newDx - this->moveState->currentDx;
    const double deltaDy = newDy - this->moveState->currentDy;

    if (std::abs(deltaDx) < 1e-6 && std::abs(deltaDy) < 1e-6) {
        return false;
    }

    this->document->lock();
    for (const auto* elem: this->moveState->elements) {
        auto* mutableElem = const_cast<Element*>(elem);
        mutableElem->move(deltaDx, deltaDy);
    }
    this->document->unlock();

    this->moveState->currentDx = newDx;
    this->moveState->currentDy = newDy;
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::endMoveSelection() -> bool {
    if (!this->moveState) {
        return false;
    }

    const double dx = this->moveState->currentDx;
    const double dy = this->moveState->currentDy;

    if (std::abs(dx) < 1e-6 && std::abs(dy) < 1e-6) {
        this->moveState.reset();
        return false;
    }

    pushHistory(QtHistoryEntry{QtMoveHistoryEntry{.pageIndex = this->moveState->pageIndex,
                                                   .elements = this->moveState->elements,
                                                   .dx = dx,
                                                   .dy = dy,
                                                   .text = "Move elements"}});
    this->moveState.reset();
    return true;
}

auto QtDocumentController::cancelMoveSelection() -> void {
    if (!this->moveState || !this->document) {
        this->moveState.reset();
        return;
    }

    const double dx = this->moveState->currentDx;
    const double dy = this->moveState->currentDy;
    if (std::abs(dx) > 1e-6 || std::abs(dy) > 1e-6) {
        this->document->lock();
        for (const auto* elem: this->moveState->elements) {
            auto* mutableElem = const_cast<Element*>(elem);
            mutableElem->move(-dx, -dy);
        }
        this->document->unlock();
        rebuildPageSnapshots();
    }
    this->moveState.reset();
}

auto QtDocumentController::isMovingSelection() const -> bool { return this->moveState.has_value(); }

auto QtDocumentController::beginScaleSelection(double originX, double originY, double startX, double startY,
                                               bool scaleX, bool scaleY, bool restoreLineWidth) -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || (!scaleX && !scaleY)) {
        return false;
    }

    if (scaleX && std::abs(startX - originX) < 1e-6) {
        return false;
    }
    if (scaleY && std::abs(startY - originY) < 1e-6) {
        return false;
    }

    bool preserveAspectRatio = false;
    bool supportMirroring = true;
    for (const auto* element: this->currentSelection->elements) {
        if (!element) {
            continue;
        }
        preserveAspectRatio = preserveAspectRatio || element->rescaleOnlyAspectRatio();
        supportMirroring = supportMirroring && element->rescaleWithMirror();
    }

    this->scaleState = QtScaleState{.originX = originX,
                                    .originY = originY,
                                    .startX = startX,
                                    .startY = startY,
                                    .currentFx = 1.0,
                                    .currentFy = 1.0,
                                    .scaleX = scaleX,
                                    .scaleY = scaleY,
                                    .preserveAspectRatio = preserveAspectRatio,
                                    .supportMirroring = supportMirroring,
                                    .restoreLineWidth = restoreLineWidth,
                                    .elements = this->currentSelection->elements,
                                    .pageIndex = this->currentSelection->pageIndex};
    return true;
}

auto QtDocumentController::updateScaleSelection(double pageX, double pageY) -> bool {
    if (!this->scaleState || !this->document) {
        return false;
    }

    constexpr double minScale = 0.02;
    auto scaleForAxis = [&](bool enabled, double current, double start, double origin) {
        if (!enabled) {
            return 1.0;
        }
        double factor = (current - origin) / (start - origin);
        if (!this->scaleState->supportMirroring) {
            factor = std::max(minScale, factor);
        } else if (std::abs(factor) < minScale) {
            factor = std::copysign(minScale, factor == 0.0 ? 1.0 : factor);
        }
        return factor;
    };

    double newFx = scaleForAxis(this->scaleState->scaleX, pageX, this->scaleState->startX, this->scaleState->originX);
    double newFy = scaleForAxis(this->scaleState->scaleY, pageY, this->scaleState->startY, this->scaleState->originY);

    if (this->scaleState->preserveAspectRatio) {
        if (this->scaleState->scaleX && this->scaleState->scaleY) {
            const double chosen = std::abs(newFx) >= std::abs(newFy) ? newFx : newFy;
            newFx = chosen;
            newFy = chosen;
        } else if (this->scaleState->scaleX) {
            newFy = newFx;
        } else if (this->scaleState->scaleY) {
            newFx = newFy;
        }
    }

    const double deltaFx = newFx / this->scaleState->currentFx;
    const double deltaFy = newFy / this->scaleState->currentFy;
    if (std::abs(deltaFx - 1.0) < 1e-6 && std::abs(deltaFy - 1.0) < 1e-6) {
        return false;
    }

    this->document->lock();
    for (const auto* elem: this->scaleState->elements) {
        auto* mutableElem = const_cast<Element*>(elem);
        mutableElem->scale(this->scaleState->originX, this->scaleState->originY, deltaFx, deltaFy, 0.0,
                           this->scaleState->restoreLineWidth);
    }
    this->document->unlock();

    this->scaleState->currentFx = newFx;
    this->scaleState->currentFy = newFy;
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::endScaleSelection() -> bool {
    if (!this->scaleState) {
        return false;
    }

    const double fx = this->scaleState->currentFx;
    const double fy = this->scaleState->currentFy;
    if (std::abs(fx - 1.0) < 1e-6 && std::abs(fy - 1.0) < 1e-6) {
        this->scaleState.reset();
        return false;
    }

    pushHistory(QtHistoryEntry{QtScaleHistoryEntry{.pageIndex = this->scaleState->pageIndex,
                                                   .elements = this->scaleState->elements,
                                                   .originX = this->scaleState->originX,
                                                   .originY = this->scaleState->originY,
                                                   .fx = fx,
                                                   .fy = fy,
                                                   .restoreLineWidth = this->scaleState->restoreLineWidth,
                                                   .text = "Scale elements"}});
    this->scaleState.reset();
    return true;
}

auto QtDocumentController::cancelScaleSelection() -> void {
    if (!this->scaleState || !this->document) {
        this->scaleState.reset();
        return;
    }

    const double fx = this->scaleState->currentFx;
    const double fy = this->scaleState->currentFy;
    if (std::abs(fx - 1.0) > 1e-6 || std::abs(fy - 1.0) > 1e-6) {
        this->document->lock();
        for (const auto* elem: this->scaleState->elements) {
            auto* mutableElem = const_cast<Element*>(elem);
            mutableElem->scale(this->scaleState->originX, this->scaleState->originY, 1.0 / fx, 1.0 / fy, 0.0,
                               this->scaleState->restoreLineWidth);
        }
        this->document->unlock();
        rebuildPageSnapshots();
    }
    this->scaleState.reset();
}

auto QtDocumentController::isScalingSelection() const -> bool { return this->scaleState.has_value(); }

auto QtDocumentController::beginVerticalSpace(std::size_t pageIndex, double pageY, bool moveAbove) -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        return false;
    }

    std::vector<const Element*> elements;
    for (const auto* element: layer->getElementsView().clone()) {
        if (!element) {
            continue;
        }
        const bool selected = moveAbove ? element->getY() + element->getElementHeight() <= pageY : element->getY() >= pageY;
        if (selected) {
            elements.push_back(element);
        }
    }

    if (elements.empty()) {
        return false;
    }

    this->verticalSpaceState = QtVerticalSpaceState{
            .startY = pageY, .currentDy = 0.0, .elements = std::move(elements), .pageIndex = pageIndex};
    return true;
}

auto QtDocumentController::updateVerticalSpace(double pageY) -> bool {
    if (!this->verticalSpaceState || !this->document) {
        return false;
    }

    const double newDy = pageY - this->verticalSpaceState->startY;
    const double deltaDy = newDy - this->verticalSpaceState->currentDy;
    if (std::abs(deltaDy) < 1e-6) {
        return false;
    }

    this->document->lock();
    for (const auto* elem: this->verticalSpaceState->elements) {
        auto* mutableElem = const_cast<Element*>(elem);
        mutableElem->move(0.0, deltaDy);
    }
    this->document->unlock();

    this->verticalSpaceState->currentDy = newDy;
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::endVerticalSpace() -> bool {
    if (!this->verticalSpaceState) {
        return false;
    }

    const double dy = this->verticalSpaceState->currentDy;
    if (std::abs(dy) < 1e-6) {
        this->verticalSpaceState.reset();
        return false;
    }

    pushHistory(QtHistoryEntry{QtMoveHistoryEntry{.pageIndex = this->verticalSpaceState->pageIndex,
                                                   .elements = this->verticalSpaceState->elements,
                                                   .dx = 0.0,
                                                   .dy = dy,
                                                   .text = "Insert vertical space"}});
    this->verticalSpaceState.reset();
    return true;
}

auto QtDocumentController::cancelVerticalSpace() -> void {
    if (!this->verticalSpaceState || !this->document) {
        this->verticalSpaceState.reset();
        return;
    }

    const double dy = this->verticalSpaceState->currentDy;
    if (std::abs(dy) > 1e-6) {
        this->document->lock();
        for (const auto* elem: this->verticalSpaceState->elements) {
            auto* mutableElem = const_cast<Element*>(elem);
            mutableElem->move(0.0, -dy);
        }
        this->document->unlock();
        rebuildPageSnapshots();
    }

    this->verticalSpaceState.reset();
}

auto QtDocumentController::isVerticalSpacing() const -> bool { return this->verticalSpaceState.has_value(); }
