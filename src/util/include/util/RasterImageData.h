/*
 * VertexNote
 *
 * Shared raster image buffer metadata for preview rendering paths.
 */

#pragma once

#include <cstddef>
#include <vector>

namespace xoj::util {

enum class RasterPixelFormat {
    Rgba8888,
    Argb32Premultiplied,
};

struct RasterImageData {
    int width = 0;
    int height = 0;
    int stride = 0;
    RasterPixelFormat format = RasterPixelFormat::Rgba8888;
    std::vector<unsigned char> pixels;

    [[nodiscard]] auto empty() const -> bool {
        return this->width <= 0 || this->height <= 0 || this->stride <= 0 || this->pixels.empty();
    }
};

}  // namespace xoj::util
