/*
 * VertexNote
 *
 * Qt document controller eraser helpers.
 */

#include "QtDocumentController.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "model/Element.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/PathParameter.h"
#include "model/Point.h"
#include "model/Stroke.h"
#include "model/eraser/PaddedBox.h"
#include "util/SmallVector.h"

auto QtDocumentController::beginErase(std::size_t pageIndex) -> void {
    this->pendingErase = QtEraseHistoryEntry{.pageIndex = pageIndex, .removedElements = {}, .text = "Erase"};
}

auto QtDocumentController::eraseAt(std::size_t pageIndex, double x, double y, double halfSize) -> int {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return 0;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return 0;
    }

    std::vector<const Element*> toRemove;
    for (auto& ep: layer->getElements()) {
        if (!ep || ep->getType() != ELEMENT_STROKE) {
            continue;
        }
        auto* s = dynamic_cast<Stroke*>(ep.get());
        if (s && s->intersects(x, y, halfSize)) {
            toRemove.push_back(s);
        }
    }

    int erased = 0;
    for (const auto* elem: toRemove) {
        auto removed = layer->removeElement(elem);
        if (removed.e) {
            if (this->pendingErase) {
                this->pendingErase->removedElements.push_back(std::move(removed));
            }
            ++erased;
        }
    }

    this->document->unlock();
    if (erased > 0) {
        rebuildPageSnapshots();
    }
    return erased;
}

auto QtDocumentController::finalizeErase() -> bool {
    if (this->pendingSegmentErase) {
        if (this->pendingSegmentErase->removedOriginals.empty()) {
            this->pendingSegmentErase.reset();
            return false;
        }
        auto entry = std::move(*this->pendingSegmentErase);
        this->pendingSegmentErase.reset();

        const auto count = entry.removedOriginals.size();
        entry.text = count == 1 ? "Segment erase stroke"
                                : "Segment erase " + std::to_string(count) + " strokes";

        pushHistory(QtHistoryEntry{std::move(entry)});
        return true;
    }

    if (!this->pendingErase || this->pendingErase->removedElements.empty()) {
        this->pendingErase.reset();
        return false;
    }

    auto entry = std::move(*this->pendingErase);
    this->pendingErase.reset();

    const auto count = entry.removedElements.size();
    entry.text = count == 1 ? "Erase stroke" : "Erase " + std::to_string(count) + " strokes";

    pushHistory(QtHistoryEntry{std::move(entry)});
    return true;
}

auto QtDocumentController::cancelErase() -> void {
    if (this->pendingSegmentErase) {
        if (this->document) {
            this->document->lock();
            for (auto pageIdx = this->pendingSegmentErase->pageIndex;
                 pageIdx < this->document->getPageCount() && !this->pendingSegmentErase->fragmentPtrs.empty();) {
                auto page = this->document->getPage(this->pendingSegmentErase->pageIndex);
                auto* layer = page ? page->getSelectedLayer() : nullptr;
                if (layer) {
                    for (const auto* frag: this->pendingSegmentErase->fragmentPtrs) {
                        layer->removeElement(frag);
                    }
                }
                break;
            }
            this->document->unlock();
        }
        this->pendingSegmentErase.reset();
        rebuildPageSnapshots();
        return;
    }
    this->pendingErase.reset();
}

auto QtDocumentController::isErasing() const -> bool {
    return this->pendingErase.has_value() || this->pendingSegmentErase.has_value();
}

auto QtDocumentController::eraseSegmentAt(std::size_t pageIndex, double x, double y, double halfSize) -> int {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return 0;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return 0;
    }

    if (!this->pendingSegmentErase) {
        this->pendingSegmentErase =
                QtSegmentEraseHistoryEntry{.pageIndex = pageIndex, .text = "Segment erase"};
    }

    const PaddedBox box{Point(x, y), halfSize, halfSize * 1.2};

    std::vector<Stroke*> candidates;
    for (auto& ep: layer->getElements()) {
        if (!ep || ep->getType() != ELEMENT_STROKE) {
            continue;
        }
        auto* s = dynamic_cast<Stroke*>(ep.get());
        if (s && s->intersects(x, y, halfSize)) {
            candidates.push_back(s);
        }
    }

    int affected = 0;
    for (auto* stroke: candidates) {
        if (stroke->getPointCount() < 2) {
            continue;
        }

        auto intersections = stroke->intersectWithPaddedBox(box);
        if (intersections.empty()) {
            continue;
        }

        const PathParameter strokeStart{0, 0.0};
        const PathParameter strokeEnd{stroke->getPointCount() - 2, 1.0};

        std::vector<std::pair<PathParameter, PathParameter>> remaining;
        PathParameter current = strokeStart;
        for (std::size_t i = 0; i + 1 < intersections.size(); i += 2) {
            if (current < intersections[i]) {
                remaining.emplace_back(current, intersections[i]);
            }
            current = intersections[i + 1];
        }
        if (current < strokeEnd) {
            remaining.emplace_back(current, strokeEnd);
        }

        if (remaining.empty()) {
            auto removed = layer->removeElement(stroke);
            if (removed.e) {
                this->pendingSegmentErase->removedOriginals.push_back(std::move(removed));
                ++affected;
            }
            continue;
        }

        if (remaining.size() == 1 && !(strokeStart < remaining[0].first) && !(remaining[0].second < strokeEnd)) {
            continue;
        }

        std::vector<std::unique_ptr<Stroke>> fragments;
        for (const auto& [lo, hi]: remaining) {
            auto frag = stroke->cloneSection(lo, hi);
            if (frag && frag->getPointCount() >= 2) {
                fragments.push_back(std::move(frag));
            }
        }

        auto removed = layer->removeElement(stroke);
        if (!removed.e) {
            continue;
        }
        this->pendingSegmentErase->removedOriginals.push_back(std::move(removed));

        for (auto& frag: fragments) {
            const auto* ptr = frag.get();
            layer->addElement(std::move(frag));
            this->pendingSegmentErase->fragmentPtrs.push_back(ptr);
        }
        ++affected;
    }

    this->document->unlock();
    if (affected > 0) {
        rebuildPageSnapshots();
    }
    return affected;
}
