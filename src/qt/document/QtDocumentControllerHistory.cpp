/*
 * VertexNote
 *
 * Qt document controller undo/redo helpers.
 */

#include "QtDocumentController.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "model/Document.h"
#include "model/Element.h"
#include "model/Layer.h"
#include "model/NotePage.h"
// ---------------------------------------------------------------------------
// Unified undo/redo
// ---------------------------------------------------------------------------

auto QtHistoryEntry::text() const -> std::string {
    return std::visit([](auto& entry) { return entry.text; }, this->data);
}

void QtDocumentController::clearHistory() {
    this->undoHistory.clear();
    this->redoHistory.clear();
}

void QtDocumentController::pushHistory(QtHistoryEntry entry) {
    this->redoHistory.clear();
    this->undoHistory.push_back(std::move(entry));
}

auto QtDocumentController::applyHistoryUndo(QtHistoryEntry& entry) -> bool {
    return std::visit(
            [this](auto& e) -> bool {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, QtGeometryHistoryEntry>) {
                    return applyGeometryHistoryEntry(e, false);
                } else if constexpr (std::is_same_v<T, QtStrokeHistoryEntry>) {
                    // Stroke undo: remove the element from the layer and take ownership
                    if (!this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    for (auto* layer: page->getLayers()) {
                        if (!layer) {
                            continue;
                        }
                        auto removed = layer->removeElement(e.element);
                        if (removed.e) {
                            e.ownedElement = std::move(removed.e);
                            e.insertionPos = removed.pos;
                            this->document->unlock();
                            rebuildPageSnapshots();
                            return true;
                        }
                    }
                    this->document->unlock();
                    return false;
                } else if constexpr (std::is_same_v<T, QtEraseHistoryEntry>) {
                    // Erase undo: re-insert all removed elements at their original positions
                    if (e.removedElements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    // Re-insert in ascending position order to restore original z-order
                    std::sort(e.removedElements.begin(), e.removedElements.end());
                    e.elementPtrs.clear();
                    for (auto& ip: e.removedElements) {
                        e.elementPtrs.push_back(ip.e.get());
                        layer->insertElement(std::move(ip.e), ip.pos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtSegmentEraseHistoryEntry>) {
                    // Segment erase undo: remove fragments, re-insert originals
                    if (e.removedOriginals.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    // Remove fragments from layer
                    e.ownedFragments.clear();
                    for (const auto* frag: e.fragmentPtrs) {
                        auto removed = layer->removeElement(frag);
                        if (removed.e) {
                            e.ownedFragments.push_back(std::move(removed));
                        }
                    }
                    e.fragmentPtrs.clear();
                    // Re-insert originals at their original positions
                    std::sort(e.removedOriginals.begin(), e.removedOriginals.end());
                    e.removedOriginalPtrs.clear();
                    for (auto& ip: e.removedOriginals) {
                        e.removedOriginalPtrs.push_back(ip.e.get());
                        layer->insertElement(std::move(ip.e), ip.pos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtMoveHistoryEntry>) {
                    // Move undo: move elements back by -dx, -dy
                    if (e.elements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    for (const auto* elem: e.elements) {
                        auto* mutableElem = const_cast<Element*>(elem);
                        mutableElem->move(-e.dx, -e.dy);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtScaleHistoryEntry>) {
                    if (e.elements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    for (const auto* elem: e.elements) {
                        auto* mutableElem = const_cast<Element*>(elem);
                        mutableElem->scale(e.originX, e.originY, 1.0 / e.fx, 1.0 / e.fy, 0.0,
                                           e.restoreLineWidth);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtTextHistoryEntry>) {
                    // Text undo: remove the text element from the layer
                    if (!this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    for (auto* layer: page->getLayers()) {
                        if (!layer) {
                            continue;
                        }
                        auto removed = layer->removeElement(e.element);
                        if (removed.e) {
                            e.ownedElement = std::move(removed.e);
                            e.insertionPos = removed.pos;
                            this->document->unlock();
                            rebuildPageSnapshots();
                            return true;
                        }
                    }
                    this->document->unlock();
                    return false;
                } else if constexpr (std::is_same_v<T, QtDeleteHistoryEntry>) {
                    // Delete undo: re-insert all removed elements
                    if (e.removedElements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    std::sort(e.removedElements.begin(), e.removedElements.end());
                    e.elementPtrs.clear();
                    for (auto& ip: e.removedElements) {
                        e.elementPtrs.push_back(ip.e.get());
                        layer->insertElement(std::move(ip.e), ip.pos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtInsertElementsHistoryEntry>) {
                    if (e.elements.empty() || !this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    e.ownedElements.clear();
                    for (auto* layer: page->getLayers()) {
                        if (!layer) {
                            continue;
                        }
                        for (const auto* ptr: e.elements) {
                            auto removed = layer->removeElement(ptr);
                            if (removed.e) {
                                e.ownedElements.push_back(std::move(removed));
                            }
                        }
                    }
                    e.elements.clear();
                    this->document->unlock();
                    if (!e.ownedElements.empty()) {
                        rebuildPageSnapshots();
                        return true;
                    }
                    return false;
                } else if constexpr (std::is_same_v<T, QtLayerTransferHistoryEntry>) {
                    if (e.records.empty() || !this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    auto& layers = page->getLayers();
                    std::vector<std::size_t> order(e.records.size());
                    std::iota(order.begin(), order.end(), 0U);
                    std::sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
                        return e.records[lhs].fromPos < e.records[rhs].fromPos;
                    });
                    for (auto recordIndex: order) {
                        auto& record = e.records[recordIndex];
                        if (record.toLayerIndex >= layers.size() || record.fromLayerIndex >= layers.size() ||
                            !layers[record.toLayerIndex] || !layers[record.fromLayerIndex]) {
                            continue;
                        }
                        auto removed = layers[record.toLayerIndex]->removeElement(record.element);
                        if (!removed.e) {
                            continue;
                        }
                        layers[record.fromLayerIndex]->insertElement(std::move(removed.e), record.fromPos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtPageSizeHistoryEntry>) {
                    if (!this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    Document::setPageSize(page, e.beforeWidth, e.beforeHeight);
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                }
            },
            entry.data);
}

auto QtDocumentController::applyHistoryRedo(QtHistoryEntry& entry) -> bool {
    return std::visit(
            [this](auto& e) -> bool {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, QtGeometryHistoryEntry>) {
                    return applyGeometryHistoryEntry(e, true);
                } else if constexpr (std::is_same_v<T, QtStrokeHistoryEntry>) {
                    // Stroke redo: re-insert the owned element at its original position
                    if (!e.ownedElement || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    e.element = e.ownedElement.get();
                    layer->insertElement(std::move(e.ownedElement), e.insertionPos);
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtEraseHistoryEntry>) {
                    // Erase redo: remove elements again using saved raw pointers
                    if (e.elementPtrs.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    e.removedElements.clear();
                    for (const auto* ptr: e.elementPtrs) {
                        auto removed = layer->removeElement(ptr);
                        if (removed.e) {
                            e.removedElements.push_back(std::move(removed));
                        }
                    }
                    e.elementPtrs.clear();
                    this->document->unlock();
                    if (!e.removedElements.empty()) {
                        rebuildPageSnapshots();
                        return true;
                    }
                    return false;
                } else if constexpr (std::is_same_v<T, QtSegmentEraseHistoryEntry>) {
                    // Segment erase redo: remove originals, re-insert fragments
                    if (e.removedOriginalPtrs.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    // Remove originals from layer
                    e.removedOriginals.clear();
                    for (const auto* ptr: e.removedOriginalPtrs) {
                        auto removed = layer->removeElement(ptr);
                        if (removed.e) {
                            e.removedOriginals.push_back(std::move(removed));
                        }
                    }
                    e.removedOriginalPtrs.clear();
                    // Re-insert fragments
                    e.fragmentPtrs.clear();
                    for (auto& ip: e.ownedFragments) {
                        e.fragmentPtrs.push_back(ip.e.get());
                        layer->addElement(std::move(ip.e));
                    }
                    e.ownedFragments.clear();
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtMoveHistoryEntry>) {
                    // Move redo: move elements by dx, dy again
                    if (e.elements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    for (const auto* elem: e.elements) {
                        auto* mutableElem = const_cast<Element*>(elem);
                        mutableElem->move(e.dx, e.dy);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtScaleHistoryEntry>) {
                    if (e.elements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    for (const auto* elem: e.elements) {
                        auto* mutableElem = const_cast<Element*>(elem);
                        mutableElem->scale(e.originX, e.originY, e.fx, e.fy, 0.0, e.restoreLineWidth);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtTextHistoryEntry>) {
                    // Text redo: re-insert the text element at its original position
                    if (!e.ownedElement || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    e.element = e.ownedElement.get();
                    layer->insertElement(std::move(e.ownedElement), e.insertionPos);
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtDeleteHistoryEntry>) {
                    // Delete redo: remove elements again using saved raw pointers
                    if (e.elementPtrs.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    e.removedElements.clear();
                    for (auto* layer: page->getLayers()) {
                        if (!layer) {
                            continue;
                        }
                        for (const auto* ptr: e.elementPtrs) {
                            auto removed = layer->removeElement(ptr);
                            if (removed.e) {
                                e.removedElements.push_back(std::move(removed));
                            }
                        }
                    }
                    e.elementPtrs.clear();
                    this->document->unlock();
                    if (!e.removedElements.empty()) {
                        rebuildPageSnapshots();
                        return true;
                    }
                    return false;
                } else if constexpr (std::is_same_v<T, QtInsertElementsHistoryEntry>) {
                    if (e.ownedElements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    std::sort(e.ownedElements.begin(), e.ownedElements.end());
                    e.elements.clear();
                    for (auto& ip: e.ownedElements) {
                        e.elements.push_back(ip.e.get());
                        layer->insertElement(std::move(ip.e), ip.pos);
                    }
                    e.ownedElements.clear();
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtLayerTransferHistoryEntry>) {
                    if (e.records.empty() || !this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    auto& layers = page->getLayers();
                    std::vector<std::size_t> order(e.records.size());
                    std::iota(order.begin(), order.end(), 0U);
                    std::sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
                        return e.records[lhs].toPos < e.records[rhs].toPos;
                    });
                    for (auto recordIndex: order) {
                        auto& record = e.records[recordIndex];
                        if (record.fromLayerIndex >= layers.size() || record.toLayerIndex >= layers.size() ||
                            !layers[record.fromLayerIndex] || !layers[record.toLayerIndex]) {
                            continue;
                        }
                        auto removed = layers[record.fromLayerIndex]->removeElement(record.element);
                        if (!removed.e) {
                            continue;
                        }
                        layers[record.toLayerIndex]->insertElement(std::move(removed.e), record.toPos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtPageSizeHistoryEntry>) {
                    if (!this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    Document::setPageSize(page, e.afterWidth, e.afterHeight);
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                }
            },
            entry.data);
}

auto QtDocumentController::canUndo() const -> bool {
    return !this->undoHistory.empty() || !this->geometryUndoHistory.empty();
}

auto QtDocumentController::canRedo() const -> bool {
    return !this->redoHistory.empty() || !this->geometryRedoHistory.empty();
}

auto QtDocumentController::undoText() const -> std::string {
    // Prefer the newest entry from either stack
    if (!this->undoHistory.empty() && !this->geometryUndoHistory.empty()) {
        return this->undoHistory.back().text();
    }
    if (!this->undoHistory.empty()) {
        return this->undoHistory.back().text();
    }
    if (!this->geometryUndoHistory.empty()) {
        return this->geometryUndoHistory.back().text;
    }
    return {};
}

auto QtDocumentController::redoText() const -> std::string {
    if (!this->redoHistory.empty()) {
        return this->redoHistory.back().text();
    }
    if (!this->geometryRedoHistory.empty()) {
        return this->geometryRedoHistory.back().text;
    }
    return {};
}

auto QtDocumentController::undo() -> bool {
    // Try unified history first, fall back to geometry-only
    if (!this->undoHistory.empty()) {
        auto entry = std::move(this->undoHistory.back());
        this->undoHistory.pop_back();
        if (applyHistoryUndo(entry)) {
            this->redoHistory.push_back(std::move(entry));
            return true;
        }
        this->undoHistory.push_back(std::move(entry));
        return false;
    }
    return undoGeometryEdit();
}

auto QtDocumentController::redo() -> bool {
    if (!this->redoHistory.empty()) {
        auto entry = std::move(this->redoHistory.back());
        this->redoHistory.pop_back();
        if (applyHistoryRedo(entry)) {
            this->undoHistory.push_back(std::move(entry));
            return true;
        }
        this->redoHistory.push_back(std::move(entry));
        return false;
    }
    return redoGeometryEdit();
}

