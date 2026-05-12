/*
 * VertexNote
 *
 * Qt document controller shape creation helpers.
 */

#include "QtDocumentController.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "model/Document.h"
#include "model/Element.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/Point.h"
#include "model/Stroke.h"
#include "model/StrokeStyle.h"
#include "model/SplineSegment.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
// ---------------------------------------------------------------------------
// Shape creation
// ---------------------------------------------------------------------------

namespace {

auto insertGeometryOnPage(Document* doc, std::size_t pageIndex,
                          std::unique_ptr<vn::geom::GeometryElement> geometry) -> const Element* {
    if (!doc || pageIndex >= doc->getPageCount()) {
        return nullptr;
    }
    doc->lock();
    auto page = doc->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        doc->unlock();
        return nullptr;
    }
    const auto* ptr = geometry.get();
    layer->addElement(std::move(geometry));
    doc->unlock();
    return ptr;
}

auto insertStrokeOnPage(Document* doc, std::size_t pageIndex, std::unique_ptr<Stroke> stroke) -> const Element* {
    if (!doc || pageIndex >= doc->getPageCount()) {
        return nullptr;
    }
    doc->lock();
    auto page = doc->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        doc->unlock();
        return nullptr;
    }
    const auto* ptr = stroke.get();
    layer->addElement(std::move(stroke));
    doc->unlock();
    return ptr;
}

void configureStrokeStyle(Stroke& stroke, Color color, double width, const std::string& lineStyle, int fill) {
    stroke.setToolType(StrokeTool::PEN);
    stroke.setColor(color);
    stroke.setWidth(width);
    if (!lineStyle.empty() && lineStyle != "plain") {
        stroke.setLineStyle(StrokeStyle::parseStyle(lineStyle));
    }
    if (fill >= 0) {
        stroke.setFill(fill);
    }
}

auto buildEllipsePoints(double x1, double y1, double x2, double y2) -> std::vector<Point> {
    const double width = x2 - x1;
    const double height = y2 - y1;
    const double radiusX = width * 0.5;
    const double radiusY = height * 0.5;
    const double centerX = x1 + radiusX;
    const double centerY = y1 + radiusY;
    const auto nbPtsPerQuadrant =
            static_cast<unsigned int>(std::ceil(5.0 + 0.3 * (std::abs(radiusX) + std::abs(radiusY))));
    const double stepAngle = M_PI_2 / std::max(1U, nbPtsPerQuadrant);

    std::vector<Point> shape;
    shape.reserve(4 * nbPtsPerQuadrant + 1);
    shape.emplace_back(centerX + radiusX, centerY);
    for (unsigned int j = 1U; j < nbPtsPerQuadrant; ++j) {
        const double tgtAngle = stepAngle * j;
        const double centerAngle = 0.25 * std::atan2(std::abs(radiusY) * std::sin(tgtAngle),
                                                     std::abs(radiusX) * std::cos(tgtAngle)) +
                                   0.75 * tgtAngle;
        shape.emplace_back(centerX + radiusX * std::cos(centerAngle), centerY + radiusY * std::sin(centerAngle));
    }
    shape.emplace_back(centerX, centerY + radiusY);

    std::vector<Point> firstHalf = shape;
    for (auto it = std::next(firstHalf.rbegin()); it != firstHalf.rend(); ++it) {
        shape.emplace_back(2 * centerX - it->x, it->y);
    }
    std::vector<Point> upperHalf = shape;
    for (auto it = std::next(upperHalf.rbegin()); it != upperHalf.rend(); ++it) {
        shape.emplace_back(it->x, 2 * centerY - it->y);
    }
    if (!shape.empty()) {
        shape.emplace_back(shape.front());
    }
    return shape;
}

auto buildArrowPoints(double x1, double y1, double x2, double y2, double thickness, bool doubleEnded)
        -> std::vector<Point> {
    const Point start(x1, y1);
    const Point end(x2, y2);
    const double lineLength = std::hypot(end.x - start.x, end.y - start.y);
    if (lineLength <= 0.0001) {
        return {start, end};
    }

    const double safeThickness = std::max(0.5, thickness);
    const double slimness = lineLength / safeThickness;
    double delta = M_PI / 6.0;
    constexpr double THICK1 = 7.0;
    constexpr double THICK3 = 1.6;
    constexpr double LENGTH2 = 0.4;
    constexpr double LENGTH4 = 0.8;
    constexpr double LENGTH4_DOUBLE = 0.5;
    double arrowDist = safeThickness * THICK1;
    if (slimness >= THICK1 / LENGTH2) {
        // keep default
    } else if (slimness >= THICK3 / LENGTH2) {
        arrowDist = lineLength * LENGTH2;
    } else if (slimness >= THICK3 / (doubleEnded ? LENGTH4_DOUBLE : LENGTH4)) {
        arrowDist = safeThickness * THICK3;
        delta = (1 + (slimness - THICK3 / LENGTH2) /
                            (THICK3 / (doubleEnded ? LENGTH4_DOUBLE : LENGTH4) - THICK3 / LENGTH2)) *
                M_PI / 6.0;
        arrowDist *= std::sin(M_PI / 6.0) / std::sin(delta);
    } else {
        arrowDist = lineLength * (doubleEnded ? LENGTH4_DOUBLE : LENGTH4);
        delta = M_PI / 3.0;
        arrowDist *= std::sin(M_PI / 6.0) / std::sin(M_PI / 3.0);
    }

    const double angle = std::atan2(end.y - start.y, end.x - start.x);
    std::vector<Point> shape;
    shape.reserve(doubleEnded ? 10 : 6);
    shape.emplace_back(start);
    if (doubleEnded) {
        shape.emplace_back(start.x + arrowDist * std::cos(angle + delta),
                           start.y + arrowDist * std::sin(angle + delta));
        shape.emplace_back(start);
        shape.emplace_back(start.x + arrowDist * std::cos(angle - delta),
                           start.y + arrowDist * std::sin(angle - delta));
        shape.emplace_back(start);
    }
    shape.emplace_back(end);
    shape.emplace_back(end.x - arrowDist * std::cos(angle + delta), end.y - arrowDist * std::sin(angle + delta));
    shape.emplace_back(end);
    shape.emplace_back(end.x - arrowDist * std::cos(angle - delta), end.y - arrowDist * std::sin(angle - delta));
    shape.emplace_back(end);
    return shape;
}

auto buildCoordinateSystemPoints(double x1, double y1, double x2, double y2) -> std::vector<Point> {
    return {Point(x1, y1), Point(x1, y2), Point(x2, y2)};
}

auto buildSplinePoints(const std::vector<std::pair<double, double>>& points) -> std::vector<Point> {
    std::vector<Point> knots;
    knots.reserve(points.size());
    for (const auto& [x, y]: points) {
        knots.emplace_back(x, y);
    }
    if (knots.size() <= 2U) {
        return knots;
    }

    std::vector<Point> result;
    result.reserve(knots.size() * 8U);
    for (std::size_t index = 0; index + 1 < knots.size(); ++index) {
        const Point& p0 = index == 0 ? knots[index] : knots[index - 1];
        const Point& p1 = knots[index];
        const Point& p2 = knots[index + 1];
        const Point& p3 = (index + 2 < knots.size()) ? knots[index + 2] : knots[index + 1];

        const Point c1(p1.x + (p2.x - p0.x) / 6.0, p1.y + (p2.y - p0.y) / 6.0);
        const Point c2(p2.x - (p3.x - p1.x) / 6.0, p2.y - (p3.y - p1.y) / 6.0);
        SplineSegment segment(p1, c1, c2, p2);
        auto segmentPoints = segment.toPointSequence();
        result.insert(result.end(), segmentPoints.begin(), segmentPoints.end());
    }
    result.emplace_back(knots.back());
    return result;
}

auto insertLegacyStroke(Document* doc, std::size_t pageIndex, const std::vector<std::pair<double, double>>& points,
                        Color color, double width, const std::string& lineStyle, std::string_view actionText)
        -> std::pair<const Element*, QtStrokeHistoryEntry> {
    if (points.size() < 2U) {
        return {nullptr, QtStrokeHistoryEntry{}};
    }

    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, -1);
    std::vector<Point> strokePoints;
    strokePoints.reserve(points.size());
    for (const auto& [x, y]: points) {
        strokePoints.emplace_back(x, y);
    }
    stroke->setPointVector(std::move(strokePoints));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(doc, pageIndex, std::move(stroke));
    QtStrokeHistoryEntry entry;
    if (ptr) {
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = std::string(actionText);
    }
    return {ptr, std::move(entry)};
}

}  // namespace

auto QtDocumentController::createLine(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color,
                                      double width, const std::string& lineStyle) -> const Element* {
    auto [ptr, entry] =
            insertLegacyStroke(this->document.get(), pageIndex, {{x1, y1}, {x2, y2}}, color, width, lineStyle, "Draw line");
    if (ptr) {
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createEdge(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color,
                                      double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto start = object.addVertex({x1, y1});
    auto end = object.addVertex({x2, y2});
    object.addLine(start, end);

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw edge";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createRectangle(std::size_t pageIndex, double x1, double y1, double x2, double y2,
                                           Color color, double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto topLeft = object.addVertex({x1, y1});
    auto topRight = object.addVertex({x2, y1});
    auto bottomRight = object.addVertex({x2, y2});
    auto bottomLeft = object.addVertex({x1, y2});
    object.addLine(topLeft, topRight);
    object.addLine(topRight, bottomRight);
    object.addLine(bottomRight, bottomLeft);
    object.addLine(bottomLeft, topLeft);

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw rectangle";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createCircle(std::size_t pageIndex, double cx, double cy, double rx, double ry, Color color,
                                        double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto center = object.addVertex({cx, cy});
    auto radiusPoint = object.addVertex({rx, ry});
    object.addEdge(vn::geom::EdgeKind::Arc, radiusPoint, radiusPoint, {center});

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw circle";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createEllipse(std::size_t pageIndex, double x1, double y1, double x2, double y2,
                                         Color color, double width, const std::string& lineStyle, int fill)
        -> const Element* {
    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, fill);
    stroke->setPointVector(buildEllipsePoints(x1, y1, x2, y2));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(this->document.get(), pageIndex, std::move(stroke));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw ellipse";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createArc(std::size_t pageIndex, double cx, double cy, double sx, double sy, double ex,
                                     double ey, Color color, double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto center = object.addVertex({cx, cy});
    auto start = object.addVertex({sx, sy});
    auto end = object.addVertex({ex, ey});
    object.addEdge(vn::geom::EdgeKind::Arc, start, end, {center});

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw arc";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createArrow(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color,
                                       double width, const std::string& lineStyle, bool doubleEnded)
        -> const Element* {
    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, -1);
    stroke->setPointVector(buildArrowPoints(x1, y1, x2, y2, width, doubleEnded));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(this->document.get(), pageIndex, std::move(stroke));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = doubleEnded ? "Draw double arrow" : "Draw arrow";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createCoordinateSystem(std::size_t pageIndex, double x1, double y1, double x2, double y2,
                                                  Color color, double width, const std::string& lineStyle)
        -> const Element* {
    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, -1);
    stroke->setPointVector(buildCoordinateSystemPoints(x1, y1, x2, y2));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(this->document.get(), pageIndex, std::move(stroke));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw coordinate system";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createSpline(std::size_t pageIndex, const std::vector<std::pair<double, double>>& points,
                                        Color color, double width, const std::string& lineStyle) -> const Element* {
    auto linearizedPoints = buildSplinePoints(points);
    if (linearizedPoints.size() < 2U) {
        return nullptr;
    }

    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, -1);
    stroke->setPointVector(std::move(linearizedPoints));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(this->document.get(), pageIndex, std::move(stroke));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw spline";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createSetsquareStroke(std::size_t pageIndex,
                                                 const std::vector<std::pair<double, double>>& points, Color color,
                                                 double width, const std::string& lineStyle) -> const Element* {
    auto [ptr, entry] =
            insertLegacyStroke(this->document.get(), pageIndex, points, color, width, lineStyle, "Draw setsquare stroke");
    if (ptr) {
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createCompassStroke(std::size_t pageIndex,
                                               const std::vector<std::pair<double, double>>& points, Color color,
                                               double width, const std::string& lineStyle) -> const Element* {
    auto [ptr, entry] =
            insertLegacyStroke(this->document.get(), pageIndex, points, color, width, lineStyle, "Draw compass stroke");
    if (ptr) {
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createPolyline(std::size_t pageIndex,
                                          const std::vector<std::pair<double, double>>& points, Color color,
                                          double width) -> const Element* {
    if (points.size() < 2U) {
        return nullptr;
    }

    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    std::vector<vn::geom::VertexId> vertices;
    vertices.reserve(points.size());
    for (const auto& [x, y]: points) {
        vertices.push_back(object.addVertex({x, y}));
    }
    for (auto it = std::next(vertices.begin()); it != vertices.end(); ++it) {
        object.addLine(*std::prev(it), *it);
    }

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw polyline";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createConstructionLine(std::size_t pageIndex, double x1, double y1, double x2, double y2,
                                                  Color color, double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto start = object.addVertex({x1, y1});
    auto end = object.addVertex({x2, y2});
    object.addEdge(vn::geom::EdgeKind::ConstructionLine, start, end);

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw construction line";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createConstructionCircle(std::size_t pageIndex, double cx, double cy, double rx, double ry,
                                                    Color color, double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto center = object.addVertex({cx, cy});
    auto radiusPoint = object.addVertex({rx, ry});
    object.addEdge(vn::geom::EdgeKind::ConstructionCircle, radiusPoint, radiusPoint, {center});

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw construction circle";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}
