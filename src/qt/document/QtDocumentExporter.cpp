/*
 * VertexNote
 *
 * Qt document export implementation (PDF via QPdfWriter, PNG via QImage).
 */

#include "QtDocumentExporter.h"

#include <cmath>

#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>

#include "QtPageContentRenderer.h"
#include "view/render/QtPainterRenderContext.h"

QtDocumentExporter::QtDocumentExporter(vn::view::render::PageContentRenderer* renderer):
        contentRenderer(renderer) {}

auto QtDocumentExporter::exportPdf(const std::filesystem::path& path,
                                   const std::vector<vn::view::render::PageRenderSnapshot>& pages,
                                   std::string* errorMessage) -> bool {
    if (!this->contentRenderer || pages.empty()) {
        if (errorMessage) {
            *errorMessage = "No pages to export.";
        }
        return false;
    }

    QPdfWriter writer(QString::fromStdString(path.string()));
    writer.setTitle(QStringLiteral("VertexNote Export"));
    writer.setCreator(QStringLiteral("VertexNote Qt Shell"));

    // Use 72 DPI so 1 page unit = 1 PDF point
    constexpr int PDF_DPI = 72;
    writer.setResolution(PDF_DPI);

    QPainter painter;
    for (std::size_t i = 0; i < pages.size(); ++i) {
        const auto& page = pages[i];
        const double pageWidth = std::max(page.width, 1.0);
        const double pageHeight = std::max(page.height, 1.0);

        // Convert page units to points (at 72 DPI, 1 unit = 1 point)
        const QPageSize pageSize(QSizeF(pageWidth, pageHeight), QPageSize::Point);
        writer.setPageSize(pageSize);
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));

        if (i == 0) {
            if (!painter.begin(&writer)) {
                if (errorMessage) {
                    *errorMessage = "Failed to open PDF file for writing.";
                }
                return false;
            }
        } else {
            writer.newPage();
        }

        painter.setRenderHint(QPainter::Antialiasing, true);

        vn::view::render::QtPainterRenderContext renderContext(&painter, 1.0);
        const vn::view::render::RenderRect renderRect{
                .x = 0.0,
                .y = 0.0,
                .width = pageWidth,
                .height = pageHeight,
        };
        this->contentRenderer->drawPage(page, renderRect, renderContext);
    }

    painter.end();
    return true;
}

auto QtDocumentExporter::exportPng(const std::filesystem::path& path,
                                   const vn::view::render::PageRenderSnapshot& page, double scale,
                                   std::string* errorMessage) -> bool {
    if (!this->contentRenderer) {
        if (errorMessage) {
            *errorMessage = "No renderer available.";
        }
        return false;
    }

    const double pageWidth = std::max(page.width, 1.0);
    const double pageHeight = std::max(page.height, 1.0);
    const int pixWidth = static_cast<int>(std::ceil(pageWidth * scale));
    const int pixHeight = static_cast<int>(std::ceil(pageHeight * scale));

    QImage image(pixWidth, pixHeight, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(scale, scale);

    vn::view::render::QtPainterRenderContext renderContext(&painter, scale);
    const vn::view::render::RenderRect renderRect{
            .x = 0.0,
            .y = 0.0,
            .width = pageWidth,
            .height = pageHeight,
    };
    this->contentRenderer->drawPage(page, renderRect, renderContext);
    painter.end();

    if (!image.save(QString::fromStdString(path.string()))) {
        if (errorMessage) {
            *errorMessage = "Failed to save PNG image.";
        }
        return false;
    }
    return true;
}

auto QtDocumentExporter::exportAllPagesPng(const std::filesystem::path& directory,
                                           const std::vector<vn::view::render::PageRenderSnapshot>& pages,
                                           double scale, std::string* errorMessage) -> bool {
    if (!this->contentRenderer || pages.empty()) {
        if (errorMessage) {
            *errorMessage = "No pages to export.";
        }
        return false;
    }

    for (std::size_t i = 0; i < pages.size(); ++i) {
        const auto filename = "page_" + std::to_string(i + 1) + ".png";
        const auto filePath = directory / filename;
        if (!exportPng(filePath, pages[i], scale, errorMessage)) {
            return false;
        }
    }
    return true;
}

auto QtDocumentExporter::printDocument(const std::vector<vn::view::render::PageRenderSnapshot>& pages,
                                       QWidget* parentWidget) -> bool {
    if (!this->contentRenderer || pages.empty()) {
        return false;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QStringLiteral("VertexNote Document"));

    QPrintDialog dialog(&printer, parentWidget);
    dialog.setWindowTitle(QStringLiteral("Print Document"));
    if (dialog.exec() != QPrintDialog::Accepted) {
        return false;
    }

    QPainter painter;
    const double printerDpi = printer.resolution();
    const double scale = printerDpi / 72.0;  // Convert from 72 DPI page units to printer DPI

    for (std::size_t i = 0; i < pages.size(); ++i) {
        const auto& page = pages[i];
        const double pageWidth = std::max(page.width, 1.0);
        const double pageHeight = std::max(page.height, 1.0);

        const QPageSize pageSize(QSizeF(pageWidth, pageHeight), QPageSize::Point);
        printer.setPageSize(pageSize);
        printer.setPageMargins(QMarginsF(0, 0, 0, 0));

        if (i == 0) {
            if (!painter.begin(&printer)) {
                return false;
            }
        } else {
            printer.newPage();
        }

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.scale(scale, scale);

        vn::view::render::QtPainterRenderContext renderContext(&painter, scale);
        const vn::view::render::RenderRect renderRect{
                .x = 0.0,
                .y = 0.0,
                .width = pageWidth,
                .height = pageHeight,
        };
        this->contentRenderer->drawPage(page, renderRect, renderContext);

        painter.resetTransform();
    }

    painter.end();
    return true;
}
