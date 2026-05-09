/*
 * VertexNote
 *
 * Experimental Qt document controller backed by the shared core model.
 */

#include "QtExperimentalDocumentController.h"

#include <algorithm>
#include <cctype>

#include "control/xojfile/LoadHandler.h"
#include "model/Document.h"
#include "model/NotePage.h"

QtExperimentalDocumentController::QtExperimentalDocumentController() { newBlankDocument(); }

void QtExperimentalDocumentController::newBlankDocument() {
    this->document = std::make_unique<Document>(&this->documentHandler);
    this->document->lock();
    this->document->addPage(std::make_shared<NotePage>(595.0, 842.0));
    this->document->unlock();
    this->loadedPath.reset();
}

auto QtExperimentalDocumentController::loadFrom(const std::filesystem::path& path, std::string* errorMessage) -> bool {
    try {
        if (isPdfPath(path)) {
            auto loaded = std::make_unique<Document>(&this->documentHandler);
            if (!loaded->readPdf(path, true, false)) {
                if (errorMessage) {
                    *errorMessage = loaded->getLastErrorMsg();
                }
                return false;
            }
            loaded->setFilepath(path);
            this->document = std::move(loaded);
            this->loadedPath = path;
            return true;
        }

        LoadHandler loader;
        auto loaded = loader.loadDocument(path);
        this->document = std::move(loaded);
        this->loadedPath = path;
        return true;
    } catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = e.what();
        }
        return false;
    }
}

auto QtExperimentalDocumentController::hasDocument() const -> bool { return static_cast<bool>(this->document); }

auto QtExperimentalDocumentController::pageCount() const -> std::size_t {
    if (!this->document) {
        return 0U;
    }
    this->document->lock_shared();
    const auto count = this->document->getPageCount();
    this->document->unlock_shared();
    return count;
}

auto QtExperimentalDocumentController::snapshotPages() const -> std::vector<QtExperimentalPageInfo> {
    std::vector<QtExperimentalPageInfo> pages;
    if (!this->document) {
        return pages;
    }

    this->document->lock_shared();
    const auto count = this->document->getPageCount();
    pages.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto page = this->document->getPage(index);
        if (!page) {
            continue;
        }

        const auto pageType = page->getBackgroundType();
        pages.push_back({.width = page->getWidth(),
                         .height = page->getHeight(),
                         .backgroundFormat = pageType.format,
                         .annotated = page->isAnnotated(),
                         .hasBackgroundName = page->backgroundHasName(),
                         .backgroundName = page->getBackgroundName(),
                         .layerCount = static_cast<std::size_t>(page->getLayerCount()),
                         .pdfPageNumber = page->getPdfPageNr()});
    }
    this->document->unlock_shared();
    return pages;
}

auto QtExperimentalDocumentController::sourcePath() const -> const std::optional<std::filesystem::path>& {
    return this->loadedPath;
}

auto QtExperimentalDocumentController::titleText() const -> std::string {
    if (this->loadedPath) {
        return this->loadedPath->filename().string();
    }
    return "Untitled Document";
}

auto QtExperimentalDocumentController::isPdfPath(const std::filesystem::path& path) -> bool {
    return normalizeExtension(path) == ".pdf";
}

auto QtExperimentalDocumentController::normalizeExtension(const std::filesystem::path& path) -> std::string {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}
