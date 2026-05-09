#include "GeometryTool.h"

#include <initializer_list>  // for initializer_list

GeometryTool::GeometryTool(double h, double r, double tx, double ty):
        height(h),
        rotation(r),
        origin(tx, ty),
        viewPool(std::make_shared<vn::util::DispatchPool<vn::view::GeometryToolView>>()) {}

GeometryTool::~GeometryTool() {}

void GeometryTool::setHeight(double height) { this->height = height; }
auto GeometryTool::getHeight() const -> double { return this->height; }

void GeometryTool::setRotation(double rotation) { this->rotation = rotation; }
auto GeometryTool::getRotation() const -> double { return this->rotation; }

auto GeometryTool::getOrigin() const -> const vn::util::Point<double>& { return origin; }

void GeometryTool::setOrigin(const vn::util::Point<double>& o) { this->origin = o; }

auto GeometryTool::getMatrix() const -> cairo_matrix_t {
    cairo_matrix_t matrix;
    cairo_matrix_init_identity(&matrix);
    cairo_matrix_translate(&matrix, this->origin.x, this->origin.y);
    cairo_matrix_rotate(&matrix, this->rotation);
    cairo_matrix_scale(&matrix, CM, CM);
    return matrix;
}

auto GeometryTool::getViewPool() const -> const std::shared_ptr<vn::util::DispatchPool<vn::view::GeometryToolView>>& {
    return viewPool;
}

auto GeometryTool::getStroke() const -> Stroke* { return this->stroke; }

void GeometryTool::setStroke(Stroke* s) { this->stroke = s; }

auto GeometryTool::computeRepaintRange(Range rg) const -> Range {
    Range lastRange = this->lastRepaintRange;
    if (this->stroke) {
        rg.addPoint(this->stroke->getX(), this->stroke->getY());
        rg.addPoint(this->stroke->getX() + this->stroke->getElementWidth(),
                    this->stroke->getY() + this->stroke->getElementHeight());
    }
    this->lastRepaintRange = rg;
    return rg.unite(lastRange);
}
