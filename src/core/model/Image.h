/*
 * VertexNote
 *
 * An Image on the document
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>      // for size_t
#include <optional>     // for optional
#include <string>       // for string
#include <string_view>  // for string_view
#include <utility>      // for pair, make_pair

#include "Element.h"  // for Element

class ObjectInputStream;
class ObjectOutputStream;


class Image: public Element {
public:
    Image();
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&&) = delete;
    Image& operator=(Image&&) = delete;
    ~Image() override = default;

public:
    void setWidth(double width);
    void setHeight(double height);

    /// Set the image data by copying the data from the provided string_view.
    void setImage(std::string_view data);

    /// Set the image data by moving the data.
    void setImage(std::string&& data);

    /// The image metadata is decoded lazily by default; call this method to validate it.
    /// Returns std::nullopt on success, an error message on failure
    std::optional<std::string> renderBuffer() const;

    void scale(double x0, double y0, double fx, double fy, double rotation, bool restoreLineWidth) override;
    void rotate(double x0, double y0, double th) override;

    auto clone() const -> ElementPtr override;

    bool hasData() const;

    /// Return a pointer to the raw data. Note that the pointer will be invalidated if the data is changed.
    const unsigned char* getRawData() const;

    /// Return the length of the raw data.
    size_t getRawDataLength() const;

    /// Return the size of the raw image, or (-1, -1) if the image has not been rendered yet.
    std::pair<int, int> getImageSize() const;

    /// Return the decoded image format name, or an empty string if the image has not been rendered yet.
    const std::string& getImageFormatName() const;

    static constexpr std::pair<int, int> NOSIZE = std::make_pair(-1, -1);

public:
    // Serialize interface
    void serialize(ObjectOutputStream& out) const override;
    void readSerialized(ObjectInputStream& in) override;

private:
    void calcSize() const override;

private:
    mutable std::pair<int, int> imageSize = {-1, -1};
    mutable std::string imageFormatName;
    mutable bool imageMetadataLoaded = false;

    std::string data;
};
