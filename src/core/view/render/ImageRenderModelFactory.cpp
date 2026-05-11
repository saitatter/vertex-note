/*
 * VertexNote
 *
 * Shared helpers for building image render models from core document data.
 */

#include "ImageRenderModelFactory.h"

#include "model/Image.h"
#include "model/TexImage.h"

namespace vn::view::render {

auto ImageRenderModelFactory::fromImage(const Image& image) -> ImageRenderModel {
    ImageRenderModel model;
    if (image.hasData() && image.getRawData() && image.getRawDataLength() > 0) {
        model.encodedBytes.assign(reinterpret_cast<const char*>(image.getRawData()), image.getRawDataLength());
    }
    model.x = image.getX();
    model.y = image.getY();
    model.width = image.getElementWidth();
    model.height = image.getElementHeight();
    return model;
}

auto ImageRenderModelFactory::fromTexImage(const TexImage& image) -> ImageRenderModel {
    ImageRenderModel model;
    model.rasterContent = image.renderPreviewRaster();
    if (model.rasterContent.empty()) {
        const auto& binaryData = image.getBinaryData();
        if (binaryData.size() >= 4 && binaryData.substr(1, 3) == "PNG") {
            model.encodedBytes = binaryData;
        }
    }
    model.x = image.getX();
    model.y = image.getY();
    model.width = image.getElementWidth();
    model.height = image.getElementHeight();
    return model;
}

}  // namespace vn::view::render
