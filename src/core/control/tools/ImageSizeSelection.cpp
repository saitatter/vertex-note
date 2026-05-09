#include "ImageSizeSelection.h"

#include "util/Rectangle.h"  // for Rectangle

using namespace vn::view;

ImageSizeSelection::ImageSizeSelection(double x, double y):
        startX(x),
        startY(y),
        endX(x),
        endY(y),
        viewPool(std::make_shared<vn::util::DispatchPool<ImageSizeSelectionView>>()) {}

void ImageSizeSelection::updatePosition(double x, double y) {
    Range old(startX, startY);
    old.addPoint(endX, endY);
    old.addPoint(x, y);
    this->endX = x;
    this->endY = y;

    this->viewPool->dispatch(vn::view::ImageSizeSelectionView::FLAG_DIRTY_REGION, old);
}

auto ImageSizeSelection::getSelectedSpace() const -> vn::util::Rectangle<double> {
    const double width = this->startX < this->endX ? this->endX - this->startX : this->startX - this->endX;
    const double height = this->startY < this->endY ? this->endY - this->startY : this->startY - this->endY;
    const double x = this->startX < this->endX ? this->startX : this->endX;
    const double y = this->startY < this->endY ? this->startY : this->endY;
    return {x, y, width, height};
}

void ImageSizeSelection::finalize() {
    Range box(startX, startY);
    box.addPoint(endX, endY);
    this->viewPool->dispatchAndClear(vn::view::ImageSizeSelectionView::FINALIZATION_REQUEST, box);
}
