/*
 * VertexNote
 *
 * Qt canvas stroke, eraser, text, and PDF selection tools.
 */

#include "QtCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QPainter>
#include <QPen>
#include <QTimer>

#include "QtPageContentRenderer.h"
#include "QtTextEditor.h"
#include "view/render/QtPainterRenderContext.h"
#include "view/render/StrokeRenderModelFactory.h"

namespace {

auto qColorFromColor(Color color, int alphaOverride = -1) -> QColor {
    return QColor(color.red, color.green, color.blue, alphaOverride >= 0 ? alphaOverride : color.alpha);
}

}  // namespace

// ---------------------------------------------------------------------------
// Stroke input
// ---------------------------------------------------------------------------

void QtCanvas::beginStrokeAtScreen(const QPointF& screenPoint, double pressure) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[*pageIdx];
    const QPointF snappedPagePoint =
            snapInputPagePoint(*pageIdx, QPointF(scenePoint.x() - pageRect.x(), scenePoint.y() - pageRect.y()));
    const double pageX = snappedPagePoint.x();
    const double pageY = snappedPagePoint.y();
    const double inputPressure = adjustedPressure(pressure);
    resetStrokeStabilizer(QPointF(pageX, pageY), inputPressure);

    Color color;
    double width;
    StrokeTool::Value toolType;
    const auto tool = this->currentToolState.activeTool;
    if (tool == QtToolType::Pen || tool == QtToolType::ShapeRecognizer || tool == QtToolType::LaserPointerPen) {
        color = this->currentToolState.penColor;
        width = this->currentToolState.penWidth;
        toolType = StrokeTool::PEN;
    } else if (tool == QtToolType::Highlighter || tool == QtToolType::LaserPointerHighlighter) {
        color = this->currentToolState.highlighterColor;
        width = this->currentToolState.highlighterWidth;
        toolType = StrokeTool::HIGHLIGHTER;
    } else {
        return;
    }

    if (this->documentController->beginStroke(*pageIdx, pageX, pageY, inputPressure, color, width, toolType,
                                               this->currentToolState.pressureSensitive,
                                               this->currentToolState.penLineStyle,
                                               this->currentToolState.fillEnabled
                                                       ? this->currentToolState.fillOpacity
                                                       : -1)) {
        this->activeStrokeStartedMs = QDateTime::currentMSecsSinceEpoch();
        this->drawing = true;
        update();
    }
}

void QtCanvas::updateStrokeAtScreen(const QPointF& screenPoint, double pressure) {
    if (!this->documentController || !this->drawing) {
        return;
    }

    const auto* active = this->documentController->activeStroke();
    if (!active) {
        return;
    }

    const auto rects = pageRects();
    if (active->pageIndex >= rects.size()) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const QRectF& pageRect = rects[active->pageIndex];
    const QPointF pagePoint(scenePoint.x() - pageRect.x(), scenePoint.y() - pageRect.y());
    const auto [stablePagePoint, pressureForStroke] = stabilizedStrokePoint(pagePoint, adjustedPressure(pressure));
    const QPointF pagePointForStroke = snapInputPagePoint(active->pageIndex, stablePagePoint);

    if (this->documentController->updateStroke(pagePointForStroke.x(), pagePointForStroke.y(), pressureForStroke)) {
        update();
    }
}

auto QtCanvas::snapInputPagePoint(std::size_t pageIndex, const QPointF& pagePoint,
                                  std::optional<vn::snap::SnapKind>* snapKind) const -> QPointF {
    if (snapKind) {
        snapKind->reset();
    }
    if (!this->documentController || (!this->geometrySnapEnabled && !this->gridSnapEnabled)) {
        return pagePoint;
    }

    const auto snapped = this->documentController->snapPagePoint(
            pageIndex, pagePoint.x(), pagePoint.y(), this->zoomFactor,
            {.geometryEnabled = this->geometrySnapEnabled,
             .gridEnabled = this->gridSnapEnabled,
             .gridSize = this->snapGridSize,
             .gridTolerance = this->snapGridTolerance,
             .screenTolerance = 22.0});
    if (snapKind) {
        *snapKind = snapped.snapKind;
    }
    return QPointF(snapped.pagePoint.x, snapped.pagePoint.y);
}

auto QtCanvas::adjustedPressure(double pressure) const -> double {
    if (!this->currentToolState.pressureSensitive) {
        return pressure;
    }
    if (pressure <= 0.0) {
        return this->pressureGuessing ? 0.5 : pressure;
    }

    const double minPressure = std::clamp(this->minimumPressure, 0.0, 0.95);
    const double normalized = std::clamp((pressure - minPressure) / std::max(0.05, 1.0 - minPressure), 0.01, 1.0);
    return std::clamp(normalized * this->pressureMultiplier, 0.01, 4.0);
}

auto QtCanvas::stabilizedStrokePoint(const QPointF& pagePoint, double pressure) -> std::pair<QPointF, double> {
    const StabilizerSample raw{.point = pagePoint, .pressure = pressure};
    this->lastRawStrokeSample = raw;
    if (!this->strokeStabilizerEnabled || this->strokeStabilizerStrength <= 0.0) {
        this->lastEmittedStrokeSample = raw;
        return {raw.point, raw.pressure};
    }

    StabilizerSample processed = raw;
    if (this->lastEmittedStrokeSample) {
        const QPointF previous = this->lastEmittedStrokeSample->point;
        if (this->strokeStabilizerPreprocessor == 1 && this->strokeStabilizerDeadzoneRadius > 0.0) {
            const QPointF movement = raw.point - previous;
            const double distance = std::hypot(movement.x(), movement.y());
            const double dot = QPointF::dotProduct(movement, this->strokeStabilizerDeadzoneDirection);
            if (distance <= this->strokeStabilizerDeadzoneRadius &&
                (!this->strokeStabilizerCuspDetection || dot >= 0.0)) {
                this->lastEmittedStrokeSample = {.point = previous, .pressure = raw.pressure};
                return {previous, raw.pressure};
            }
            if (distance > 0.0) {
                const double ratio = std::min(this->strokeStabilizerDeadzoneRadius / distance, 1.0);
                processed.point = raw.point - movement * ratio;
                this->strokeStabilizerDeadzoneDirection = movement;
            }
        } else if (this->strokeStabilizerPreprocessor == 2) {
            const QPointF spring((raw.point.x() - previous.x()) / this->strokeStabilizerMass,
                                 (raw.point.y() - previous.y()) / this->strokeStabilizerMass);
            this->strokeStabilizerVelocity = this->strokeStabilizerVelocity * (1.0 - this->strokeStabilizerDrag) + spring;
            processed.point = previous + this->strokeStabilizerVelocity;
        }
    }

    this->strokeStabilizerSamplesBuffer.push_back(processed);
    const auto maxSamples = static_cast<std::size_t>(std::max(2, this->strokeStabilizerSamples));
    if (this->strokeStabilizerSamplesBuffer.size() > maxSamples) {
        this->strokeStabilizerSamplesBuffer.erase(this->strokeStabilizerSamplesBuffer.begin(),
                                                  this->strokeStabilizerSamplesBuffer.end() -
                                                          static_cast<std::ptrdiff_t>(maxSamples));
    }

    QPointF averaged = processed.point;
    double averagedPressure = processed.pressure;
    if (this->strokeStabilizerAveragingMethod != 0) {
        averaged = QPointF();
        averagedPressure = 0.0;
        double weightSum = 0.0;
        double distanceSum = 0.0;
        for (auto it = this->strokeStabilizerSamplesBuffer.rbegin(); it != this->strokeStabilizerSamplesBuffer.rend();
             ++it) {
            double weight = 1.0;
            if (this->strokeStabilizerAveragingMethod == 2) {
                weight = std::exp(-(distanceSum * distanceSum) /
                                  (2.0 * this->strokeStabilizerSigma * this->strokeStabilizerSigma));
                if (weight < 0.01) {
                    break;
                }
                const auto next = std::next(it);
                if (next != this->strokeStabilizerSamplesBuffer.rend()) {
                    distanceSum += std::hypot(it->point.x() - next->point.x(), it->point.y() - next->point.y());
                }
            }
            averaged += it->point * weight;
            averagedPressure += it->pressure * weight;
            weightSum += weight;
        }
        averaged /= std::max(weightSum, 0.001);
        averagedPressure /= std::max(weightSum, 0.001);
    }

    const double strength = std::clamp(this->strokeStabilizerStrength, 0.0, 1.0);
    const StabilizerSample emitted{
            .point = processed.point * (1.0 - strength) + averaged * strength,
            .pressure = processed.pressure * (1.0 - strength) + averagedPressure * strength,
    };
    this->lastEmittedStrokeSample = emitted;
    return {emitted.point, emitted.pressure};
}

void QtCanvas::resetStrokeStabilizer(const QPointF& pagePoint, double pressure) {
    const StabilizerSample sample{.point = pagePoint, .pressure = pressure};
    this->strokeStabilizerSamplesBuffer.clear();
    this->strokeStabilizerSamplesBuffer.push_back(sample);
    this->lastRawStrokeSample = sample;
    this->lastEmittedStrokeSample = sample;
    this->strokeStabilizerVelocity = QPointF();
    this->strokeStabilizerDeadzoneDirection = QPointF();
}

void QtCanvas::maybeFinalizeStabilizedStroke() {
    if (!this->documentController || !this->strokeStabilizerEnabled || !this->strokeStabilizerFinalizeStroke ||
        !this->lastRawStrokeSample || !this->lastEmittedStrokeSample) {
        return;
    }

    const auto raw = *this->lastRawStrokeSample;
    const auto emitted = *this->lastEmittedStrokeSample;
    const bool samePoint = std::abs(raw.point.x() - emitted.point.x()) < 0.01 &&
                           std::abs(raw.point.y() - emitted.point.y()) < 0.01;
    const bool samePressure = std::abs(raw.pressure - emitted.pressure) < 0.001;
    if (!samePoint || !samePressure) {
        this->documentController->updateStroke(raw.point.x(), raw.point.y(), raw.pressure);
    }
}

void QtCanvas::finalizeActiveStroke() {
    if (!this->documentController || !this->drawing) {
        return;
    }

    const auto tool = this->currentToolState.activeTool;
    std::optional<std::size_t> activePageIndex;
    if (const auto* active = this->documentController->activeStroke()) {
        activePageIndex = active->pageIndex;
    }
    bool added = false;
    if (tool == QtToolType::LaserPointerPen || tool == QtToolType::LaserPointerHighlighter) {
        maybeFinalizeStabilizedStroke();
        if (const auto* active = this->documentController->activeStroke(); active && active->stroke) {
            this->laserOverlayStrokes.push_back({.pageIndex = active->pageIndex,
                                                 .model = vn::view::render::StrokeRenderModelFactory::fromStroke(*active->stroke),
                                                 .createdMs = QDateTime::currentMSecsSinceEpoch()});
            if (this->laserFadeTimer) {
                this->laserFadeTimer->start();
            }
        }
        this->documentController->cancelStroke();
    } else {
        maybeFinalizeStabilizedStroke();
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (shouldFilterActiveStroke(nowMs)) {
            if (this->trySelectOnStrokeFiltered) {
                if (const auto* active = this->documentController->activeStroke();
                    active && active->stroke && active->stroke->getPointCount() > 0U) {
                    const auto point = active->stroke->getPoint(active->stroke->getPointCount() - 1U);
                    this->documentController->selectElementAt(active->pageIndex, point.x, point.y,
                                                              10.0 / std::max(0.001, this->zoomFactor));
                    Q_EMIT selectionStateChanged();
                }
            }
            this->lastFilteredStrokeMs = nowMs;
            this->documentController->cancelStroke();
            Q_EMIT statusHintChanged(this->doActionOnStrokeFiltered
                                             ? QStringLiteral("Stroke filtered; legacy floating toolbox unavailable")
                                             : QStringLiteral("Stroke filtered"));
        } else {
            added = this->documentController->finalizeStroke(tool == QtToolType::ShapeRecognizer,
                                                             this->shapeRecognizerMinSize,
                                                             this->snapRecognizedShapesEnabled);
        }
    }
    this->drawing = false;
    this->activeStrokeStartedMs = 0;
    this->activeTouchPointId = -1;
    this->strokeStabilizerSamplesBuffer.clear();
    this->lastRawStrokeSample.reset();
    this->lastEmittedStrokeSample.reset();
    update();
    if (added) {
        if (activePageIndex) {
            maybeAppendEmptyLastPageOnDraw(*activePageIndex, added);
        }
        Q_EMIT documentEdited();
    }
}

auto QtCanvas::shouldFilterActiveStroke(qint64 nowMs) -> bool {
    if (!this->strokeFilterEnabled || !this->documentController || this->strokeFilterIgnoreLengthMm <= 0.0) {
        return false;
    }
    if (this->strokeFilterSuccessiveTimeMs > 0 && this->lastFilteredStrokeMs > 0 &&
        nowMs - this->lastFilteredStrokeMs <= this->strokeFilterSuccessiveTimeMs) {
        return false;
    }

    const auto* active = this->documentController->activeStroke();
    if (!active || !active->stroke) {
        return false;
    }
    const auto pointCount = active->stroke->getPointCount();
    if (pointCount == 0) {
        return true;
    }

    double lengthPoints = 0.0;
    for (std::size_t index = 1; index < pointCount; ++index) {
        const Point previous = active->stroke->getPoint(index - 1U);
        const Point current = active->stroke->getPoint(index);
        lengthPoints += std::hypot(current.x - previous.x, current.y - previous.y);
    }
    constexpr double millimetersPerPoint = 25.4 / 72.0;
    const double lengthMm = lengthPoints * millimetersPerPoint;
    const qint64 durationMs = this->activeStrokeStartedMs > 0 ? nowMs - this->activeStrokeStartedMs : 0;
    return durationMs <= this->strokeFilterIgnoreTimeMs && lengthMm <= this->strokeFilterIgnoreLengthMm;
}

void QtCanvas::maybeAppendEmptyLastPageOnDraw(std::size_t pageIndex, bool strokeAdded) {
    if (!strokeAdded || this->emptyLastPageAppendMode != "onDrawOfLastPage" || !this->documentController ||
        this->documentController->hasPdfBackgroundDocument()) {
        return;
    }
    const auto count = this->documentController->pageCount();
    if (count > 0U && pageIndex + 1U == count) {
        this->documentController->addPageAfter(pageIndex);
    }
}

auto QtCanvas::maybeAppendEmptyLastPageOnScroll() -> bool {
    if (this->emptyLastPageAppendMode != "onScrollOfLastPage" || !this->documentController ||
        this->documentController->hasPdfBackgroundDocument()) {
        return false;
    }
    const auto count = this->documentController->pageCount();
    if (count == 0U || currentPageIndex() + 1U != count) {
        return false;
    }

    const QRectF bounds = documentSceneBounds();
    const double visibleBottom = (this->scrollY + height() / std::max(0.001, this->zoomFactor)) * this->zoomFactor;
    const double documentBottom = bounds.bottom() * this->zoomFactor;
    if (std::abs(documentBottom - visibleBottom) < 5.0) {
        this->documentController->addPageAfter(count - 1U);
        return true;
    }
    return false;
}

void QtCanvas::cancelActiveStroke() {
    if (this->documentController) {
        this->documentController->cancelStroke();
    }
    this->drawing = false;
    this->activeStrokeStartedMs = 0;
    this->activeTouchPointId = -1;
    this->strokeStabilizerSamplesBuffer.clear();
    this->lastRawStrokeSample.reset();
    this->lastEmittedStrokeSample.reset();
    update();
}

void QtCanvas::drawActiveStroke(QPainter& painter) const {
    const auto* active = this->documentController ? this->documentController->activeStroke() : nullptr;
    if (!active || !active->stroke || !this->pageContentRenderer) {
        return;
    }

    const auto rects = pageRects();
    if (active->pageIndex >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[active->pageIndex];
    painter.save();
    painter.setClipRect(pageRect);

    // Translate so the stroke renderer draws at page position
    painter.translate(pageRect.x(), pageRect.y());

    auto model = vn::view::render::StrokeRenderModelFactory::fromStroke(*active->stroke);
    vn::view::render::QtPainterRenderContext renderContext(&painter, this->zoomFactor);
    this->pageContentRenderer->drawStroke(model, renderContext);

    painter.restore();
}

void QtCanvas::drawLaserPointerStrokes(QPainter& painter) const {
    if (this->laserOverlayStrokes.empty() || !this->pageContentRenderer) {
        return;
    }

    const auto rects = pageRects();
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (const auto& overlay: this->laserOverlayStrokes) {
        if (overlay.pageIndex >= rects.size()) {
            continue;
        }

        const auto age = std::max<qint64>(0, now - overlay.createdMs);
        const double alpha = std::clamp(1.0 - static_cast<double>(age) / this->laserPointerFadeOutMs, 0.0, 1.0);
        if (alpha <= 0.0) {
            continue;
        }

        auto model = overlay.model;
        model.color.alpha = static_cast<uint8_t>(std::round(static_cast<double>(model.color.alpha) * alpha));
        if (model.highlighter && model.fill > 0) {
            model.fill = static_cast<int>(std::round(static_cast<double>(model.fill) * alpha));
        }

        const QRectF& pageRect = rects[overlay.pageIndex];
        painter.save();
        painter.setClipRect(pageRect);
        painter.translate(pageRect.x(), pageRect.y());

        vn::view::render::QtPainterRenderContext renderContext(&painter, this->zoomFactor);
        this->pageContentRenderer->drawStroke(model, renderContext);
        painter.restore();
    }
}

void QtCanvas::pruneLaserPointerStrokes() {
    if (this->laserOverlayStrokes.empty()) {
        return;
    }

    const auto now = QDateTime::currentMSecsSinceEpoch();
    std::erase_if(this->laserOverlayStrokes, [this, now](const QtLaserOverlayStroke& overlay) {
        return now - overlay.createdMs >= this->laserPointerFadeOutMs;
    });
}

// ---------------------------------------------------------------------------
// Eraser input
// ---------------------------------------------------------------------------

void QtCanvas::beginEraseAtScreen(const QPointF& screenPoint) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    this->documentController->beginErase(*pageIdx);
    this->erasing = true;
    updateEraserPreviewAtScreen(screenPoint);

    // Immediately erase at the press point
    const QRectF& pageRect = rects[*pageIdx];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();
    const double halfSize = currentEraserHalfSize();

    const int erased = usesMaskEraser() ? this->documentController->eraseSegmentAt(*pageIdx, pageX, pageY, halfSize)
                                        : this->documentController->eraseAt(*pageIdx, pageX, pageY, halfSize);
    if (erased > 0) {
        update();
        Q_EMIT documentEdited();
    }
}

void QtCanvas::eraseAtScreen(const QPointF& screenPoint) {
    if (!this->documentController || !this->erasing) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[*pageIdx];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();
    const double halfSize = currentEraserHalfSize();

    updateEraserPreviewAtScreen(screenPoint);

    const int erased = usesMaskEraser() ? this->documentController->eraseSegmentAt(*pageIdx, pageX, pageY, halfSize)
                                        : this->documentController->eraseAt(*pageIdx, pageX, pageY, halfSize);
    if (erased > 0) {
        update();
        Q_EMIT documentEdited();
    }
}

void QtCanvas::finalizeErase() {
    if (!this->documentController) {
        this->erasing = false;
        return;
    }
    this->documentController->finalizeErase();
    this->erasing = false;
    if (!this->temporaryRightButtonEraser && this->currentToolState.activeTool != QtToolType::Eraser) {
        clearEraserPreview();
    }
    Q_EMIT documentEdited();
}

void QtCanvas::cancelErase() {
    if (this->documentController) {
        this->documentController->cancelErase();
    }
    this->erasing = false;
    this->temporaryRightButtonEraser = false;
    clearEraserPreview();
}

auto QtCanvas::usesMaskEraser() const -> bool {
    return this->currentToolState.eraserMode != QtEraserMode::DeleteStroke;
}

auto QtCanvas::currentEraserHalfSize() const -> double {
    return (this->currentToolState.eraserWidth * 1.35) / 2.0;
}

void QtCanvas::updateEraserPreviewAtScreen(const QPointF& screenPoint) {
    const bool shouldShow = !this->spaceHeld &&
                            (this->currentToolState.activeTool == QtToolType::Eraser || this->temporaryRightButtonEraser);
    if (!shouldShow) {
        clearEraserPreview();
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        clearEraserPreview();
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        clearEraserPreview();
        return;
    }

    const QRectF& pageRect = rects[*pageIdx];
    this->eraserPreviewPageIndex = *pageIdx;
    this->eraserPreviewPagePoint = QPointF(scenePoint.x() - pageRect.x(), scenePoint.y() - pageRect.y());
    update();
}

void QtCanvas::clearEraserPreview() {
    if (!this->eraserPreviewPageIndex) {
        return;
    }
    this->eraserPreviewPageIndex.reset();
    update();
}

void QtCanvas::drawEraserPreview(QPainter& painter) const {
    if (!this->eraserPreviewPageIndex) {
        return;
    }

    const auto rects = pageRects();
    if (*this->eraserPreviewPageIndex >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[*this->eraserPreviewPageIndex];
    const double halfSize = currentEraserHalfSize();
    const QRectF eraserRect(pageRect.x() + this->eraserPreviewPagePoint.x() - halfSize,
                            pageRect.y() + this->eraserPreviewPagePoint.y() - halfSize, halfSize * 2.0,
                            halfSize * 2.0);

    QPen border(QColor(40, 40, 40, 255));
    border.setWidthF(1.4 / std::max(this->zoomFactor, 0.0001));
    painter.save();
    painter.setPen(border);
    painter.setBrush(QColor(255, 255, 255, 255));
    painter.drawRect(eraserRect);
    painter.restore();
}

// ---------------------------------------------------------------------------
// Text editing
// ---------------------------------------------------------------------------

void QtCanvas::beginTextEditAtScreen(const QPointF& screenPoint) {
    if (!this->documentController) {
        return;
    }

    // Commit any existing text edit first
    commitTextEdit();

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[*pageIdx];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();

    // Create text editor if needed
    if (!this->textEditor) {
        this->textEditor = new QtTextEditor(this);
        this->textEditor->setTabOptions(this->useSpacesForTab, this->numberOfSpacesForTab);
        connect(this->textEditor, &QtTextEditor::editingFinished, this, [this](bool committed) {
            if (committed && this->textEditor->isNewText()) {
                auto textElem = this->textEditor->newTextElement();
                if (textElem) {
                    this->documentController->insertTextElement(this->textEditor->editedPageIndex(),
                                                                std::move(textElem));
                    update();
                    Q_EMIT documentEdited();
                }
            } else if (committed) {
                update();
                Q_EMIT documentEdited();
            }
        });
    }

    // Check if clicking on an existing text element
    const double hitRadius = 20.0 / this->zoomFactor;
    auto* existingText = this->documentController->hitTestTextElement(*pageIdx, pageX, pageY, hitRadius);

    if (existingText) {
        this->textEditor->beginEditing(existingText, pageRect, this->zoomFactor);
    } else {
        this->textEditor->beginNewText(*pageIdx, pageX, pageY, pageRect, this->zoomFactor,
                                       this->currentToolState.penColor, "Sans", 12.0);
    }
}

void QtCanvas::commitTextEdit() {
    if (this->textEditor && this->textEditor->isEditing()) {
        this->textEditor->commit();
    }
}

void QtCanvas::cancelTextEdit() {
    if (this->textEditor && this->textEditor->isEditing()) {
        this->textEditor->cancel();
    }
}

void QtCanvas::beginPdfTextSelectionAtScreen(const QPointF& screenPoint) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[*pageIdx];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();
    const auto style = this->currentToolState.activeTool == QtToolType::PdfTextRect ? PdfPageSelectionStyle::Area
                                                                                    : PdfPageSelectionStyle::Linear;
    if (this->documentController->beginPdfTextSelection(*pageIdx, pageX, pageY, style)) {
        this->pdfTextSelecting = true;
        update();
    }
}

void QtCanvas::updatePdfTextSelectionAtScreen(const QPointF& screenPoint) {
    if (!this->documentController || !this->pdfTextSelecting) {
        return;
    }

    const auto* selection = this->documentController->pdfTextSelection() ? &*this->documentController->pdfTextSelection() : nullptr;
    if (!selection) {
        return;
    }

    const auto rects = pageRects();
    if (selection->pageIndex >= rects.size()) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const QRectF& pageRect = rects[selection->pageIndex];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();
    if (this->documentController->updatePdfTextSelection(pageX, pageY)) {
        update();
    }
}

void QtCanvas::finalizePdfTextSelection() {
    if (!this->documentController || !this->pdfTextSelecting) {
        this->pdfTextSelecting = false;
        return;
    }

    const auto selectedText = this->documentController->finalizePdfTextSelection();
    this->pdfTextSelecting = false;
    if (!selectedText.empty()) {
        QApplication::clipboard()->setText(QString::fromStdString(selectedText));
        Q_EMIT statusHintChanged(QStringLiteral("Copied selected PDF text"));
    } else {
        Q_EMIT statusHintChanged(QStringLiteral("No PDF text selected"));
    }
    update();
}

void QtCanvas::cancelPdfTextSelection() {
    if (this->documentController) {
        this->documentController->cancelPdfTextSelection();
    }
    this->pdfTextSelecting = false;
    update();
}

void QtCanvas::drawCursorHighlight(QPainter& painter) const {
    if (!this->cursorHighlightEnabled || !this->cursorHighlightVisible) {
        return;
    }

    painter.save();
    const QPointF scenePoint = screenToScene(this->lastCursorScreenPosition);
    const double radius = static_cast<double>(this->cursorHighlightRadiusPixels) / std::max(0.001, this->zoomFactor);
    const double borderWidth =
            static_cast<double>(this->cursorHighlightBorderWidthPixels) / std::max(0.001, this->zoomFactor);
    painter.setBrush(qColorFromColor(this->cursorHighlightFill));
    painter.setPen(borderWidth > 0.0 ? QPen(qColorFromColor(this->cursorHighlightBorder), borderWidth)
                                     : Qt::NoPen);
    painter.drawEllipse(scenePoint, radius, radius);
    painter.restore();
}

void QtCanvas::drawPdfTextSelectionOverlay(QPainter& painter) const {
    const auto* selection = this->documentController ? (this->documentController->pdfTextSelection()
                                                                ? &*this->documentController->pdfTextSelection()
                                                                : nullptr)
                                                     : nullptr;
    if (!selection) {
        return;
    }

    const auto rects = pageRects();
    if (selection->pageIndex >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[selection->pageIndex];
    painter.save();
    painter.translate(pageRect.x(), pageRect.y());

    QColor fill(255, 230, 90, std::clamp(this->currentToolState.pdfTextMarkerOpacity, 0, 255));
    QColor outline(220, 170, 0, 190);
    painter.setPen(QPen(outline, 1.0 / this->zoomFactor));
    painter.setBrush(fill);

    if (!selection->previewRects.empty()) {
        for (const auto& rect: selection->previewRects) {
            const QRectF qrect(QPointF(rect.x1, rect.y1), QPointF(rect.x2, rect.y2));
            painter.drawRect(qrect.normalized());
        }
    } else {
        const QRectF qrect(QPointF(selection->bounds.x1, selection->bounds.y1),
                           QPointF(selection->bounds.x2, selection->bounds.y2));
        painter.drawRect(qrect.normalized());
    }

    painter.restore();
}

