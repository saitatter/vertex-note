/*
 * VertexNote
 *
 * A TexImage on the document
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>
#include <string>  // for string

#include <glib.h>  // for GError

#include "util/RasterImageData.h"

#include "Element.h"  // for Element

class ObjectInputStream;
class ObjectOutputStream;

namespace poppler {
class document;
}


class TexImage: public Element {
public:
    TexImage();
    TexImage(const TexImage&) = delete;
    TexImage& operator=(const TexImage&) = delete;
    TexImage(const TexImage&&) = delete;
    TexImage&& operator=(const TexImage&&) = delete;
    ~TexImage() override;

public:
    void setWidth(double width);
    void setHeight(double height);

    /**
     * Returns the binary data (PDF or PNG (deprecated)).
     */
    const std::string& getBinaryData() const;

    /**
     * @return The PDF Document, if rendered as a PDF.
     */
    const poppler::document* getPdf() const;

    [[nodiscard]] auto renderPreviewRaster() const -> xoj::util::RasterImageData;

    void scale(double x0, double y0, double fx, double fy, double rotation, bool restoreLineWidth) override;
    void rotate(double x0, double y0, double th) override;

    // text tag to alow latex
    void setText(std::string text);
    std::string getText() const;

    auto cloneTexImage() const -> std::unique_ptr<TexImage>;
    auto clone() const -> ElementPtr override;

    /**
     * @return true if the binary data (PNG or PDF) was loaded successfully.
     */
    bool loadData(std::string&& bytes, GError** err = nullptr);

public:
    // Serialize interface
    void serialize(ObjectOutputStream& out) const override;
    void readSerialized(ObjectInputStream& in) override;

private:
    void calcSize() const override;

    /**
     * Free PDF
     */
    void freeImageAndPdf();

private:
    /**
     * TeX PDF Document, if rendered as PDF
     */
    std::shared_ptr<poppler::document> pdf;

    /**
     * PNG Image / PDF Document
     */
    std::string binaryData;

    /**
     * Tex String
     */
    std::string text;
};
