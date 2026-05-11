/*
 * VertexNote
 *
 * Qt document controller layer management helpers.
 */

#include "QtDocumentController.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "model/Element.h"
#include "model/Layer.h"
#include "model/NotePage.h"

auto QtDocumentController::layerCount(std::size_t pageIndex) const -> std::size_t {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return 0;
    }
    auto page = this->document->getPage(pageIndex);
    return page ? static_cast<std::size_t>(page->getLayerCount()) : 0;
}

auto QtDocumentController::layerInfos(std::size_t pageIndex) const -> std::vector<QtLayerInfo> {
    std::vector<QtLayerInfo> infos;
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return infos;
    }
    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return infos;
    }

    const auto selectedId = page->getSelectedLayerId();
    const auto& layers = page->getLayers();
    infos.reserve(layers.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto* layer = layers[i];
        if (!layer) {
            continue;
        }
        QtLayerInfo info;
        info.index = i;
        info.name = layer->hasName() ? layer->getName() : "Layer " + std::to_string(i + 1);
        info.visible = layer->isVisible();
        info.selected = (static_cast<std::size_t>(selectedId) == i);
        info.elementCount = layer->getElementsView().size();
        infos.push_back(std::move(info));
    }
    return infos;
}

auto QtDocumentController::selectedLayerIndex(std::size_t pageIndex) const -> std::size_t {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return 0;
    }
    auto page = this->document->getPage(pageIndex);
    return page ? static_cast<std::size_t>(page->getSelectedLayerId()) : 0;
}

void QtDocumentController::selectLayer(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex));
    }
    this->document->unlock();
}

void QtDocumentController::setLayerVisible(std::size_t pageIndex, std::size_t layerIndex, bool visible) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex < layers.size() && layers[layerIndex]) {
            layers[layerIndex]->setVisible(visible);
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::addLayer(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto* newLayer = new Layer();
        auto& layers = page->getLayers();
        layers.push_back(newLayer);
        page->setSelectedLayerId(static_cast<Layer::Index>(layers.size() - 1));
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::removeLayer(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layers.size() > 1 && layerIndex < layers.size()) {
            delete layers[layerIndex];
            layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(layerIndex));
            if (static_cast<std::size_t>(page->getSelectedLayerId()) >= layers.size()) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layers.size() - 1));
            }
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::renameLayer(std::size_t pageIndex, std::size_t layerIndex, const std::string& name) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex < layers.size() && layers[layerIndex]) {
            layers[layerIndex]->setName(name);
        }
    }
    this->document->unlock();
}

void QtDocumentController::moveLayerUp(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex + 1 < layers.size()) {
            std::swap(layers[layerIndex], layers[layerIndex + 1]);
            if (static_cast<std::size_t>(page->getSelectedLayerId()) == layerIndex) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex + 1));
            } else if (static_cast<std::size_t>(page->getSelectedLayerId()) == layerIndex + 1) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex));
            }
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::moveLayerDown(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex > 0) {
            std::swap(layers[layerIndex], layers[layerIndex - 1]);
            if (static_cast<std::size_t>(page->getSelectedLayerId()) == layerIndex) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex - 1));
            } else if (static_cast<std::size_t>(page->getSelectedLayerId()) == layerIndex - 1) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex));
            }
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::copyLayer(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex < layers.size()) {
            Layer* cloned = layers[layerIndex]->clone();
            layers.insert(layers.begin() + static_cast<std::ptrdiff_t>(layerIndex) + 1, cloned);
            page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex + 1));
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::mergeLayerDown(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    if (layerIndex == 0) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex < layers.size()) {
            Layer* srcLayer = layers[layerIndex];
            Layer* dstLayer = layers[layerIndex - 1];
            auto elements = srcLayer->clearNoFree();
            for (auto& elem: elements) {
                dstLayer->addElement(std::move(elem));
            }
            delete srcLayer;
            layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(layerIndex));
            page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex - 1));
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::showAllLayers(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    auto page = this->document->getPage(pageIndex);
    if (page) {
        for (auto* layer: page->getLayers()) {
            if (layer) {
                layer->setVisible(true);
            }
        }
    }
    rebuildPageSnapshots();
}

void QtDocumentController::hideAllLayers(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    auto page = this->document->getPage(pageIndex);
    if (page) {
        for (auto* layer: page->getLayers()) {
            if (layer) {
                layer->setVisible(false);
            }
        }
    }
    rebuildPageSnapshots();
}

auto QtDocumentController::isPageAnnotated(std::size_t pageIndex) const -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }
    auto page = this->document->getPage(pageIndex);
    return page && page->isAnnotated();
}

auto QtDocumentController::canMoveSelectionToAdjacentLayer(int direction) const -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document ||
        this->currentSelection->pageIndex >= this->document->getPageCount() || direction == 0) {
        return false;
    }

    auto page = this->document->getPage(this->currentSelection->pageIndex);
    if (!page) {
        return false;
    }

    const auto& layers = page->getLayers();
    for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
        const auto* layer = layers[layerIndex];
        if (!layer) {
            continue;
        }
        for (const auto* element: this->currentSelection->elements) {
            if (layer->indexOf(element) == Element::InvalidIndex) {
                continue;
            }

            if (direction > 0 && layerIndex + 1 < layers.size()) {
                return true;
            }
            if (direction < 0 && layerIndex > 0) {
                return true;
            }
        }
    }

    return false;
}

auto QtDocumentController::moveSelectionToAdjacentLayer(int direction) -> bool {
    if (!canMoveSelectionToAdjacentLayer(direction)) {
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(this->currentSelection->pageIndex);
    if (!page) {
        this->document->unlock();
        return false;
    }

    auto& layers = page->getLayers();
    std::vector<QtLayerTransferRecord> records;
    for (const auto* element: this->currentSelection->elements) {
        for (std::size_t fromLayerIndex = 0; fromLayerIndex < layers.size(); ++fromLayerIndex) {
            auto* sourceLayer = layers[fromLayerIndex];
            if (!sourceLayer) {
                continue;
            }

            const auto fromPos = sourceLayer->indexOf(element);
            if (fromPos == Element::InvalidIndex) {
                continue;
            }

            const auto toLayerIndex = direction > 0 ? fromLayerIndex + 1 : fromLayerIndex - 1;
            if (toLayerIndex >= layers.size() || !layers[toLayerIndex]) {
                break;
            }

            auto* targetLayer = layers[toLayerIndex];
            const auto toPos = static_cast<Element::Index>(targetLayer->getElementsView().size());
            auto removed = sourceLayer->removeElement(element);
            if (!removed.e) {
                break;
            }
            const auto* movedPtr = removed.e.get();
            targetLayer->insertElement(std::move(removed.e), toPos);
            records.push_back(QtLayerTransferRecord{.element = movedPtr,
                                                    .fromLayerIndex = fromLayerIndex,
                                                    .toLayerIndex = toLayerIndex,
                                                    .fromPos = fromPos,
                                                    .toPos = toPos});
            break;
        }
    }
    this->document->unlock();

    if (records.empty()) {
        return false;
    }

    pushHistory(QtHistoryEntry{QtLayerTransferHistoryEntry{
            .pageIndex = this->currentSelection->pageIndex,
            .records = std::move(records),
            .text = direction > 0 ? "Move selection layer up" : "Move selection layer down",
    }});
    rebuildPageSnapshots();
    return true;
}
