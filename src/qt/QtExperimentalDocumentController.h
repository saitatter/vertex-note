/*
 * VertexNote
 *
 * Experimental Qt document controller backed by the shared core model.
 */

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "model/Document.h"
#include "model/DocumentHandler.h"
#include "view/render/Renderers.h"

struct QtExperimentalPageInfo {
    double width = 0.0;
    double height = 0.0;
    vn::view::render::PageBackgroundRenderModel background;
};

class QtExperimentalDocumentController {
public:
    QtExperimentalDocumentController();

public:
    void newBlankDocument();
    auto loadFrom(const std::filesystem::path& path, std::string* errorMessage = nullptr) -> bool;

    [[nodiscard]] auto hasDocument() const -> bool;
    [[nodiscard]] auto pageCount() const -> std::size_t;
    [[nodiscard]] auto snapshotPages() const -> std::vector<QtExperimentalPageInfo>;
    [[nodiscard]] auto sourcePath() const -> const std::optional<std::filesystem::path>&;
    [[nodiscard]] auto titleText() const -> std::string;

private:
    static auto isPdfPath(const std::filesystem::path& path) -> bool;
    static auto normalizeExtension(const std::filesystem::path& path) -> std::string;

private:
    DocumentHandler documentHandler;
    std::unique_ptr<Document> document;
    std::optional<std::filesystem::path> loadedPath;
};
