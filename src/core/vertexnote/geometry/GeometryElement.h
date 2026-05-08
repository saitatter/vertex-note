/*
 * VertexNote
 *
 * Xournal++ element wrapper for object-based geometry.
 */

#pragma once

#include <memory>

#include "model/Element.h"
#include "vertexnote/geometry/GeometryObject.h"

class Stroke;

namespace vn::geom {

class GeometryElement final: public Element {
public:
    GeometryElement();
    explicit GeometryElement(GeometryObject object);

    [[nodiscard]] auto geometry() -> GeometryObject&;
    [[nodiscard]] auto geometry() const -> const GeometryObject&;

    void setStrokeWidth(double width);
    [[nodiscard]] auto getStrokeWidth() const -> double;

    [[nodiscard]] auto makeStrokeFallback() const -> std::unique_ptr<Stroke>;
    [[nodiscard]] auto setVertexPosition(VertexId id, Vec2 position) -> bool;

    void move(double dx, double dy) override;
    void scale(double x0, double y0, double fx, double fy, double rotation, bool restoreLineWidth) override;
    void rotate(double x0, double y0, double th) override;
    [[nodiscard]] auto distanceTo(double x, double y) const -> double override;
    [[nodiscard]] auto clone() const -> ElementPtr override;
    void serialize(ObjectOutputStream& out) const override;
    void readSerialized(ObjectInputStream& in) override;

protected:
    void calcSize() const override;

private:
    GeometryObject object;
    double strokeWidth = 1.0;
};

}  // namespace vn::geom
