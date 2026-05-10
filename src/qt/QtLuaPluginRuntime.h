/*
 * VertexNote
 *
 * Minimal Lua plugin runtime for the Qt shell.
 */

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>

#include "ui/common/IPluginUiBridge.h"

class QtDocumentController;
class QWidget;
namespace vn::ui::common {
class ICommandHost;
}

class QtLuaPluginRuntime {
public:
    struct PluginStatus {
        std::string name;
        std::string description;
        std::string author;
        std::string version;
        std::filesystem::path path;
        bool valid = false;
        bool defaultEnabled = false;
        bool enabled = false;
        int registeredActions = 0;
        std::string error;
    };

public:
    QtLuaPluginRuntime(vn::ui::common::IPluginUiBridge* bridge, vn::ui::common::ICommandHost* commandHost,
                       QWidget* parent);
    ~QtLuaPluginRuntime();

public:
    void loadEnabledPlugins();
    void configureDocumentAccess(QtDocumentController* controller, std::function<std::size_t()> currentPageProvider,
                                 std::function<void(std::size_t)> pageNavigator,
                                 std::function<void()> refreshUi, std::function<void()> markDirty);
    void configureExportAccess(std::function<bool(const std::filesystem::path&, std::string*)> pdfExporter,
                               std::function<bool(const std::filesystem::path&, std::string*)> pngExporter);
    void configureToolAccess(std::function<void(uint32_t, const std::string&, bool)> toolColorChanger);
    void saveEnabledStates(const std::vector<std::pair<std::string, bool>>& states);
    [[nodiscard]] auto statuses() const -> std::vector<PluginStatus>;
    [[nodiscard]] auto parentWidget() const -> QWidget*;
    [[nodiscard]] auto documentControllerPtr() const -> QtDocumentController*;
    [[nodiscard]] auto currentDocumentPageIndex() const -> std::size_t;
    void navigateToDocumentPage(std::size_t pageIndex) const;
    void refreshDocumentUi() const;
    void markDocumentDirty() const;
    [[nodiscard]] auto exportPdf(const std::filesystem::path& path, std::string* errorMessage) const -> bool;
    [[nodiscard]] auto exportPng(const std::filesystem::path& path, std::string* errorMessage) const -> bool;
    void changeToolColor(uint32_t rgb, const std::string& tool, bool selection) const;

public:
    struct Plugin;

private:
    vn::ui::common::IPluginUiBridge* bridge = nullptr;
    vn::ui::common::ICommandHost* commandHost = nullptr;
    QWidget* parent = nullptr;
    QtDocumentController* documentController = nullptr;
    std::function<std::size_t()> currentPageProvider;
    std::function<void(std::size_t)> pageNavigator;
    std::function<void()> refreshUi;
    std::function<void()> markDirty;
    std::function<bool(const std::filesystem::path&, std::string*)> pdfExporter;
    std::function<bool(const std::filesystem::path&, std::string*)> pngExporter;
    std::function<void(uint32_t, const std::string&, bool)> toolColorChanger;
    std::vector<std::unique_ptr<Plugin>> plugins;
};
