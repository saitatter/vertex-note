/*
 * VertexNote
 *
 * Draw object-based geometry elements.
 */

#pragma once

#include "View.h"

namespace vn::geom {
class GeometryElement;
}

class vn::view::GeometryElementView: public vn::view::ElementView {
public:
    explicit GeometryElementView(const vn::geom::GeometryElement* geometry);
    ~GeometryElementView() override = default;

    void draw(const Context& ctx) const override;

private:
    const vn::geom::GeometryElement* geometry;
};
