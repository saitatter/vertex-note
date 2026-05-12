/*
 * VertexNote
 *
 * Qt Lua plugin runtime accessors and shell callbacks.
 */

#include "QtLuaPluginRuntime.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

#include "ui/common/ICommandHost.h"

void QtLuaPluginRuntime::configureDocumentAccess(QtDocumentController* controller,
                                                 std::function<std::size_t()> currentPageProvider,
                                                 std::function<void(std::size_t)> pageNavigator,
                                                 std::function<void()> refreshUi, std::function<void()> markDirty) {
    this->documentController = controller;
    this->currentPageProvider = std::move(currentPageProvider);
    this->pageNavigator = std::move(pageNavigator);
    this->refreshUi = std::move(refreshUi);
    this->markDirty = std::move(markDirty);
}

void QtLuaPluginRuntime::configureExportAccess(
        std::function<bool(const std::filesystem::path&, std::string*)> pdfExporter,
        std::function<bool(const std::filesystem::path&, std::string*)> pngExporter) {
    this->pdfExporter = std::move(pdfExporter);
    this->pngExporter = std::move(pngExporter);
}

void QtLuaPluginRuntime::configureToolAccess(
        std::function<void(uint32_t, const std::string&, bool)> toolColorChanger,
        std::function<QtToolState()> toolStateProvider) {
    this->toolColorChanger = std::move(toolColorChanger);
    this->toolStateProvider = std::move(toolStateProvider);
}

void QtLuaPluginRuntime::configureColorPaletteAccess(std::function<std::vector<QtPaletteColor>()> colorPaletteProvider) {
    this->colorPaletteProvider = std::move(colorPaletteProvider);
}

void QtLuaPluginRuntime::configureViewAccess(std::function<double()> zoomProvider,
                                             std::function<void(double)> zoomSetter,
                                             std::function<int()> layoutSpanProvider) {
    this->zoomProvider = std::move(zoomProvider);
    this->zoomSetter = std::move(zoomSetter);
    this->layoutSpanProvider = std::move(layoutSpanProvider);
}

void QtLuaPluginRuntime::configureDisplayAccess(std::function<int()> displayDpiProvider) {
    this->displayDpiProvider = std::move(displayDpiProvider);
}

void QtLuaPluginRuntime::configureViewportAccess(
        std::function<vn::ui::common::CanvasViewport()> viewportProvider,
        std::function<void(double, double, bool)> viewportScroller) {
    this->viewportProvider = std::move(viewportProvider);
    this->viewportScroller = std::move(viewportScroller);
}

void QtLuaPluginRuntime::configureSidebarAccess(std::function<int()> sidebarPageProvider,
                                                std::function<void(int)> sidebarPageSetter) {
    this->sidebarPageProvider = std::move(sidebarPageProvider);
    this->sidebarPageSetter = std::move(sidebarPageSetter);
}

void QtLuaPluginRuntime::configureFloatingToolboxAccess(std::function<void(double, double)> floatingToolboxShower) {
    this->floatingToolboxShower = std::move(floatingToolboxShower);
}

void QtLuaPluginRuntime::configureFontAccess(std::function<std::pair<std::string, double>()> fontProvider,
                                             std::function<void(std::string, double)> fontSetter) {
    this->fontProvider = std::move(fontProvider);
    this->fontSetter = std::move(fontSetter);
}

void QtLuaPluginRuntime::configureFileAccess(std::function<bool(const std::filesystem::path&, int)> fileOpener) {
    this->fileOpener = std::move(fileOpener);
}

auto QtLuaPluginRuntime::parentWidget() const -> QWidget* { return this->parent; }

auto QtLuaPluginRuntime::documentControllerPtr() const -> QtDocumentController* { return this->documentController; }

auto QtLuaPluginRuntime::currentDocumentPageIndex() const -> std::size_t {
    return this->currentPageProvider ? this->currentPageProvider() : 0U;
}

void QtLuaPluginRuntime::navigateToDocumentPage(std::size_t pageIndex) const {
    if (this->pageNavigator) {
        this->pageNavigator(pageIndex);
    }
}

void QtLuaPluginRuntime::refreshDocumentUi() const {
    if (this->refreshUi) {
        this->refreshUi();
    }
}

void QtLuaPluginRuntime::markDocumentDirty() const {
    if (this->markDirty) {
        this->markDirty();
    }
}

auto QtLuaPluginRuntime::exportPdf(const std::filesystem::path& path, std::string* errorMessage) const -> bool {
    if (!this->pdfExporter) {
        if (errorMessage) {
            *errorMessage = "Qt PDF export is not available";
        }
        return false;
    }
    return this->pdfExporter(path, errorMessage);
}

auto QtLuaPluginRuntime::exportPng(const std::filesystem::path& path, std::string* errorMessage) const -> bool {
    if (!this->pngExporter) {
        if (errorMessage) {
            *errorMessage = "Qt PNG export is not available";
        }
        return false;
    }
    return this->pngExporter(path, errorMessage);
}

void QtLuaPluginRuntime::changeToolColor(uint32_t rgb, const std::string& tool, bool selection) const {
    if (this->toolColorChanger) {
        this->toolColorChanger(rgb, tool, selection);
    }
}

auto QtLuaPluginRuntime::currentToolState() const -> QtToolState {
    return this->toolStateProvider ? this->toolStateProvider() : QtToolState{};
}

auto QtLuaPluginRuntime::currentColorPalette() const -> std::vector<QtPaletteColor> {
    return this->colorPaletteProvider ? this->colorPaletteProvider() : qtDefaultColorPalette();
}

auto QtLuaPluginRuntime::currentZoom() const -> double { return this->zoomProvider ? this->zoomProvider() : 1.0; }

void QtLuaPluginRuntime::setZoom(double zoom) const {
    if (this->zoomSetter) {
        this->zoomSetter(zoom);
    }
}

auto QtLuaPluginRuntime::currentLayoutSpan() const -> int {
    return this->layoutSpanProvider ? this->layoutSpanProvider() : 1;
}

auto QtLuaPluginRuntime::currentDisplayDpi() const -> int {
    if (this->displayDpiProvider) {
        const int configuredDpi = this->displayDpiProvider();
        if (configuredDpi > 0) {
            return configuredDpi;
        }
    }

    QScreen* screen = this->parent ? this->parent->screen() : QGuiApplication::primaryScreen();
    return static_cast<int>(std::lround(screen ? screen->logicalDotsPerInch() : 96.0));
}

auto QtLuaPluginRuntime::currentViewport() const -> vn::ui::common::CanvasViewport {
    return this->viewportProvider ? this->viewportProvider() : vn::ui::common::CanvasViewport{};
}

void QtLuaPluginRuntime::scrollViewportTo(double x, double y, bool relative) const {
    if (this->viewportScroller) {
        this->viewportScroller(x, y, relative);
    }
}

auto QtLuaPluginRuntime::currentSidebarPage() const -> int {
    return this->sidebarPageProvider ? std::max(1, this->sidebarPageProvider()) : 1;
}

void QtLuaPluginRuntime::setSidebarPage(int pageNo) const {
    if (this->sidebarPageSetter) {
        this->sidebarPageSetter(std::max(1, pageNo));
    }
}

void QtLuaPluginRuntime::showFloatingToolbox(double x, double y) const {
    if (this->floatingToolboxShower) {
        this->floatingToolboxShower(x, y);
    }
}

auto QtLuaPluginRuntime::currentFont() const -> std::pair<std::string, double> {
    return this->fontProvider ? this->fontProvider() : std::pair<std::string, double>{"Sans", 12.0};
}

void QtLuaPluginRuntime::setFont(std::string name, double size) const {
    if (this->fontSetter) {
        this->fontSetter(std::move(name), size);
    }
}

auto QtLuaPluginRuntime::openFile(const std::filesystem::path& path, int pageIndex) const -> bool {
    return this->fileOpener ? this->fileOpener(path, pageIndex) : false;
}

auto QtLuaPluginRuntime::commandChecked(std::string_view commandId) const -> bool {
    return this->commandHost && this->commandHost->hasCommand(commandId) && this->commandHost->isCommandChecked(commandId);
}

void QtLuaPluginRuntime::setCommandEnabled(std::string_view commandId, bool enabled) const {
    if (this->commandHost && this->commandHost->hasCommand(commandId)) {
        this->commandHost->setCommandEnabled(commandId, enabled);
    }
}
