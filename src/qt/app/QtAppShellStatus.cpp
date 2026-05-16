/*
 * VertexNote
 *
 * Qt app shell status and footer synchronization.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <cmath>

#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace {

auto geometryModeLabel(QtGeometrySelectionMode mode) -> QString {
    switch (mode) {
        case QtGeometrySelectionMode::Vertex:
            return QStringLiteral("Vertex");
        case QtGeometrySelectionMode::Edge:
            return QStringLiteral("Edge");
        case QtGeometrySelectionMode::Face:
            return QStringLiteral("Face");
        case QtGeometrySelectionMode::Object:
            return QStringLiteral("Object");
    }
    return QStringLiteral("Geometry");
}

auto toolModeLabel(QtToolType tool) -> QString {
    switch (tool) {
        case QtToolType::Pen:
            return QStringLiteral("Pen");
        case QtToolType::Eraser:
            return QStringLiteral("Eraser");
        case QtToolType::Highlighter:
            return QStringLiteral("Highlighter");
        case QtToolType::Text:
            return QStringLiteral("Text");
        case QtToolType::PdfTextLinear:
            return QStringLiteral("PDF text");
        case QtToolType::PdfTextRect:
            return QStringLiteral("PDF area");
        case QtToolType::SelectRect:
            return QStringLiteral("Select");
        case QtToolType::SelectRegion:
            return QStringLiteral("Region");
        case QtToolType::SelectObject:
            return QStringLiteral("Object");
        case QtToolType::Hand:
            return QStringLiteral("Hand");
        case QtToolType::VerticalSpace:
            return QStringLiteral("Space");
        case QtToolType::DrawPolyline:
            return QStringLiteral("Polyline");
        case QtToolType::DrawEdge:
            return QStringLiteral("Edge");
        case QtToolType::DrawRectangle:
            return QStringLiteral("Rectangle");
        default:
            return QStringLiteral("Tool");
    }
}

auto topologyStatusText(QtGeometryFaceLoopStatusKind loopKind, std::size_t faceCount, std::size_t diagonalCount)
        -> QString {
    if (faceCount > 0U) {
        if (faceCount == 1U && diagonalCount > 0U) {
            return QStringLiteral("Face %1 diag").arg(static_cast<int>(diagonalCount));
        }
        return faceCount == 1U ? QStringLiteral("1 face")
                               : QStringLiteral("%1 faces").arg(static_cast<int>(faceCount));
    }

    switch (loopKind) {
        case QtGeometryFaceLoopStatusKind::Ready:
            return QStringLiteral("Loop ready");
        case QtGeometryFaceLoopStatusKind::AlreadyFilled:
            return QStringLiteral("Face exists");
        case QtGeometryFaceLoopStatusKind::OpenOrBranching:
            return QStringLiteral("Loop open");
        case QtGeometryFaceLoopStatusKind::NeedMoreEdges:
            return QStringLiteral("Need 3 edges");
        case QtGeometryFaceLoopStatusKind::UnsupportedEdge:
            return QStringLiteral("Lines only");
        case QtGeometryFaceLoopStatusKind::NoEdges:
            return QStringLiteral("Pick edges");
        case QtGeometryFaceLoopStatusKind::NoSelection:
            return QStringLiteral("-");
    }

    return QStringLiteral("-");
}

}  // namespace

void QtAppShell::syncFooterWidgets() {
    const auto pageCount = this->documentController.pageCount();
    const auto currentPage = this->window.canvas()->currentPageIndex();

    if (auto* pageSpin = this->window.footerPageSpin()) {
        const QSignalBlocker blocker(pageSpin);
        pageSpin->setMinimum(1);
        pageSpin->setMaximum(static_cast<int>(std::max<std::size_t>(pageCount, 1U)));
        pageSpin->setSuffix(QStringLiteral(" / %1").arg(std::max<std::size_t>(pageCount, 1U)));
        pageSpin->setValue(static_cast<int>(std::min(currentPage + 1, std::max<std::size_t>(pageCount, 1U))));
    }

    if (auto* layerCombo = this->window.footerLayerCombo()) {
        const QSignalBlocker blocker(layerCombo);
        layerCombo->clear();
        if (pageCount > 0) {
            const auto infos = this->documentController.layerInfos(currentPage);
            int currentIndex = -1;
            for (int comboIndex = 0; comboIndex < static_cast<int>(infos.size()); ++comboIndex) {
                const auto& info = infos[static_cast<std::size_t>(comboIndex)];
                layerCombo->addItem(QString::fromStdString(info.name),
                                    QVariant::fromValue(static_cast<qulonglong>(info.index)));
                if (info.selected) {
                    currentIndex = comboIndex;
                }
            }
            if (currentIndex >= 0) {
                layerCombo->setCurrentIndex(currentIndex);
            }
        }
    }

    if (auto* zoomSlider = this->window.footerZoomSlider()) {
        const QSignalBlocker blocker(zoomSlider);
        const auto zoomPercent =
                static_cast<int>(std::round(this->window.canvas()->sessionViewportState().zoom * 100.0));
        zoomSlider->setValue(std::clamp(zoomPercent, zoomSlider->minimum(), zoomSlider->maximum()));
    }
}

void QtAppShell::updateWindowTitle() {
    std::string title = "VertexNote - ";
    if (this->currentSettings.showFilePathInTitlebar && this->documentController.sourcePath()) {
        title += this->documentController.sourcePath()->string();
    } else {
        title += this->documentController.titleText();
    }
    if (this->currentSettings.showPageNumberInTitlebar && this->documentController.pageCount() > 0U) {
        title += " - Page " + std::to_string(this->window.canvas()->currentPageIndex() + 1U) + "/" +
                 std::to_string(this->documentController.pageCount());
    }
    if (this->session.isDirty()) {
        title += " *";
    }
    setMainWindowTitle(title);
}

void QtAppShell::updateStatusBarLabels() {
    const auto pageIdx = this->window.canvas()->currentPageIndex();
    const auto pageCount = this->documentController.pageCount();
    this->window.pageStatusLabel()->setText(
            QStringLiteral("Page %1 of %2").arg(pageIdx + 1).arg(pageCount > 0 ? pageCount : 1));

    if (pageCount > 0) {
        const auto layerIdx = this->documentController.selectedLayerIndex(pageIdx);
        const auto layerCount = this->documentController.layerCount(pageIdx);
        this->window.layerStatusLabel()->setText(QStringLiteral("Layer %1 / %2").arg(layerIdx + 1).arg(layerCount));
    }

    const auto zoom = this->window.canvas()->sessionViewportState().zoom;
    this->window.zoomStatusLabel()->setText(QStringLiteral("%1%").arg(zoom * 100.0, 0, 'f', 0));

    auto* canvas = this->window.canvas();
    QStringList snapFlags;
    if (canvas->isGeometrySnapEnabled()) {
        snapFlags << QStringLiteral("V");
    }
    if (canvas->isGridSnapEnabled()) {
        snapFlags << QStringLiteral("Grid");
    }
    if (canvas->isRotationSnapEnabled()) {
        snapFlags << QStringLiteral("Angle");
    }

    QStringList viewFlags;
    if (canvas->isGeometryWireframeViewEnabled()) {
        viewFlags << QStringLiteral("Wire");
    }
    if (canvas->isGeometryVertexOverlayEnabled()) {
        viewFlags << QStringLiteral("Verts");
    }
    if (canvas->isGeometryLinkedVertexOverlayEnabled()) {
        viewFlags << QStringLiteral("Linked");
    }
    if (!canvas->isGeometryFaceFillVisible()) {
        viewFlags << QStringLiteral("No faces");
    }

    QStringList panelViewFlags = viewFlags;
    if (canvas->isGeometryFaceFillVisible()) {
        panelViewFlags << QStringLiteral("Faces");
    }

    QStringList selectionFlags;
    const auto vertexCount = this->documentController.selectedVertexIds().size();
    const auto edgeCount = this->documentController.selectedEdgeIds().size();
    const auto faceCount = this->documentController.selectedFaceIds().size();
    if (vertexCount > 0U) {
        selectionFlags << QStringLiteral("%1V").arg(static_cast<int>(vertexCount));
    }
    if (edgeCount > 0U) {
        selectionFlags << QStringLiteral("%1E").arg(static_cast<int>(edgeCount));
    }
    if (faceCount > 0U) {
        selectionFlags << QStringLiteral("%1F").arg(static_cast<int>(faceCount));
    }
    if (selectionFlags.isEmpty() && this->documentController.selectedGeometry()) {
        selectionFlags << QStringLiteral("Obj");
    }

    const QString modeText = activeWorkspaceId() == "notes" ? toolModeLabel(canvas->activeTool())
                                                            : geometryModeLabel(canvas->toolState().geometrySelectionMode);
    const QString snapText = snapFlags.isEmpty() ? QStringLiteral("-") : snapFlags.join(QLatin1Char(' '));
    const QString selectionText =
            selectionFlags.isEmpty() ? QStringLiteral("-") : selectionFlags.join(QLatin1Char(' '));
    const QString panelViewText =
            panelViewFlags.isEmpty() ? QStringLiteral("-") : panelViewFlags.join(QLatin1Char(' '));
    const auto loopStatus = this->documentController.selectedGeometryFaceLoopStatus();
    const QString topologyText =
            topologyStatusText(loopStatus.kind, faceCount, this->documentController.selectedGeometryFaceSplitDiagonals().size());
    QString depthText = QStringLiteral("-");
    double modelX = 0.0;
    double modelY = 0.0;
    double modelZ = 0.0;
    bool modelEditorEnabled = false;
    if (const auto range = this->documentController.selectedGeometryModelRange()) {
        depthText = std::abs(range->minZ - range->maxZ) <= 1e-6
                            ? QStringLiteral("Z %1").arg(range->minZ, 0, 'f', 1)
                            : QStringLiteral("Z %1..%2").arg(range->minZ, 0, 'f', 1).arg(range->maxZ, 0, 'f', 1);
        modelX = (range->minX + range->maxX) * 0.5;
        modelY = (range->minY + range->maxY) * 0.5;
        modelZ = (range->minZ + range->maxZ) * 0.5;
        modelEditorEnabled = true;
    }

    QString text = QStringLiteral("Geometry: %1 | Snap: %2 | Sel: %3").arg(modeText, snapText, selectionText);
    if (!viewFlags.isEmpty()) {
        text += QStringLiteral(" | View: %1").arg(viewFlags.join(QLatin1Char(' ')));
    }
    this->window.geometryStatusLabel()->setText(text);
    this->window.geometryPanel()->setStatusSummary(modeText, snapText, selectionText, panelViewText,
                                                   canvas->geometryProjectionViewName(), depthText);
    this->window.geometryPanel()->setTopologySummary(topologyText);
    this->window.geometryPanel()->setModelInspector(modelX, modelY, modelZ, modelEditorEnabled);
}
