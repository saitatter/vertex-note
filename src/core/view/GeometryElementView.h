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

class xoj::view::GeometryElementView: public xoj::view::ElementView {
public:
    explicit GeometryElementView(const vn::geom::GeometryElement* geometry);
    ~GeometryElementView() override = default;

    void draw(const Context& ctx) const override;

private:
    const vn::geom::GeometryElement* geometry;
};
