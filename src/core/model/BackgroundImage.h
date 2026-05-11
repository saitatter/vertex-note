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

#include <gio/gio.h>                // for GInputStream
#include <glib.h>                   // for GError

#include "filesystem.h"  // for path
#include "util/RasterImageData.h"

typedef struct _GdkPixbuf GdkPixbuf;

struct BackgroundImage {
    friend bool operator==(const BackgroundImage& lhs, const BackgroundImage& rhs) = default;

    void free();

    void loadFile(fs::path const& filepath, GError** error);
    void loadFile(GInputStream* stream, fs::path const& filepath, GError** error);

    int getCloneId() const;
    void setCloneId(int id);
    void clearSaveState();

    fs::path getFilepath() const;
    void setFilepath(fs::path filepath);

    bool isAttached() const;
    void setAttach(bool attach);

    [[nodiscard]] auto renderPreviewRaster() const -> xoj::util::RasterImageData;

    bool isEmpty() const;

private:
    struct Content;

    std::shared_ptr<Content> img;

    friend GdkPixbuf* getBackgroundImagePixbuf(BackgroundImage& image);
    friend const GdkPixbuf* getBackgroundImagePixbuf(const BackgroundImage& image);
};

GdkPixbuf* getBackgroundImagePixbuf(BackgroundImage& image);
const GdkPixbuf* getBackgroundImagePixbuf(const BackgroundImage& image);
bool saveBackgroundImagePng(const BackgroundImage& image, const fs::path& path);
