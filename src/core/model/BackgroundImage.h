/*
 * VertexNote
 *
 * A background image of a page
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for shared_ptr
#include <string>
#include <string_view>

#include <glib.h>  // for GError

#include "filesystem.h"  // for path
#include "util/RasterImageData.h"

struct BackgroundImage {
    friend bool operator==(const BackgroundImage& lhs, const BackgroundImage& rhs) = default;

    void free();

    void loadFile(fs::path const& filepath, GError** error);
    void loadFile(std::string_view bytes, fs::path const& filepath, GError** error);

    int getCloneId() const;
    void setCloneId(int id);
    void clearSaveState();

    fs::path getFilepath() const;
    void setFilepath(fs::path filepath);

    bool isAttached() const;
    void setAttach(bool attach);

    [[nodiscard]] auto renderPreviewRaster() const -> xoj::util::RasterImageData;
    [[nodiscard]] auto hasLoadedImage() const -> bool;

    bool isEmpty() const;

private:
    struct Content;

    std::shared_ptr<Content> img;

    friend bool saveBackgroundImagePng(const BackgroundImage& image, const fs::path& path);
};

bool saveBackgroundImagePng(const BackgroundImage& image, const fs::path& path);
