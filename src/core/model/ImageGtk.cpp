#include "ImageGtk.h"

#include "model/Image.h"

namespace vn::legacy {

std::optional<std::string> setImageFromGdkPixbuf(Image& image, GdkPixbuf* pixbuf) {
    if (!pixbuf) {
        return "No image data";
    }

    gchar* buffer = nullptr;
    gsize bufferSize = 0;
    GError* error = nullptr;
    if (!gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &bufferSize, "png", &error, nullptr)) {
        std::string message = error && error->message ? error->message : "Could not encode image as PNG";
        if (error) {
            g_error_free(error);
        }
        return message;
    }

    image.setImage(std::string(buffer, bufferSize));
    g_free(buffer);
    return std::nullopt;
}

}  // namespace vn::legacy
