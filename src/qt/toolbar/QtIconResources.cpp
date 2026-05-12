/*
 * VertexNote
 *
 * Qt icon lookup and command icon registry.
 */

#include "QtIconResources.h"

#include "QtMainWindow.h"
#include "config-paths.h"

#include <array>
#include <filesystem>
#include <utility>

#include <QAction>
#include <QSize>
#include <QStyle>
#include <QString>
#include <QToolButton>
#include <QWidget>

namespace {

namespace fs = std::filesystem;

std::string gBundledIconTheme = "color";
std::string gBundledIconTone = "light";

}  // namespace

void setQtIconAppearance(std::string theme, std::string tone) {
    gBundledIconTheme = std::move(theme);
    gBundledIconTone = std::move(tone);
}

auto bundledQtIcon(std::string_view fileName) -> QIcon {
    const auto tryPath = [&](const fs::path& path) -> QIcon {
        return QIcon(QString::fromStdString(path.string()));
    };

    const auto preferredFamily = gBundledIconTheme == "lucide" ? std::string("iconsLucide") : std::string("iconsColor");
    const auto fallbackFamily = gBundledIconTheme == "lucide" ? std::string("iconsColor") : std::string("iconsLucide");
    const auto preferredTone = gBundledIconTone == "dark" ? std::string("dark") : std::string("light");
    const auto fallbackTone = preferredTone == "dark" ? std::string("light") : std::string("dark");
    const std::array<std::string, 4> themes = {{
            preferredFamily + "-" + preferredTone,
            preferredFamily + "-" + fallbackTone,
            fallbackFamily + "-" + preferredTone,
            fallbackFamily + "-" + fallbackTone,
    }};

    for (const auto& theme: themes) {
        for (const auto& sizeDir: {"24x24", "scalable"}) {
            const fs::path candidate =
                    fs::path(PROJECT_SOURCE_DIR) / "ui" / theme / "hicolor" / sizeDir / "actions" /
                    std::string(fileName);
            if (!fs::exists(candidate)) {
                continue;
            }

            const auto icon = tryPath(candidate);
            if (!icon.isNull()) {
                return icon;
            }
        }
    }

    return QIcon();
}

auto bundledQtNamedIcon(std::string_view logicalName) -> QIcon {
    return bundledQtIcon("xopp-" + std::string(logicalName) + ".svg");
}

auto themeSymbolicIcon(std::string_view iconBaseName) -> QIcon {
    const fs::path symbolicPath =
            fs::path("C:/msys64/mingw64/share/icons/Adwaita/symbolic/actions") /
            (std::string(iconBaseName) + "-symbolic.svg");
    if (fs::exists(symbolicPath)) {
        return QIcon(QString::fromStdString(symbolicPath.string()));
    }
    return QIcon::fromTheme(QString::fromUtf8(iconBaseName.data(), static_cast<int>(iconBaseName.size())));
}

auto createStaticIconWidget(QWidget* parent, std::string_view iconFile, std::string_view tooltip) -> QToolButton* {
    auto* button = new QToolButton(parent);
    button->setAutoRaise(true);
    button->setEnabled(false);
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolTip(QString::fromUtf8(tooltip.data(), static_cast<int>(tooltip.size())));
    button->setIcon(bundledQtIcon(iconFile));
    button->setIconSize(QSize(20, 20));
    button->setFixedSize(26, 26);
    return button;
}

void applyQtCommandIcons(QtMainWindow& window) {
    const auto setStandardIcon = [&](std::string_view id, QStyle::StandardPixmap sp) {
        if (auto* action = window.commandHost()->actionForCommand(id)) {
            action->setIcon(window.style()->standardIcon(sp));
        }
    };
    const auto setNamedIcon = [&](std::string_view id, std::string_view logicalName) {
        if (auto* action = window.commandHost()->actionForCommand(id)) {
            auto icon = bundledQtNamedIcon(logicalName);
            if (!icon.isNull()) {
                action->setIcon(icon);
            }
        }
    };
    const auto setThemeIcon = [&](std::string_view id, std::string_view themeName) {
        if (auto* action = window.commandHost()->actionForCommand(id)) {
            auto icon = themeSymbolicIcon(themeName);
            if (!icon.isNull()) {
                action->setIcon(icon);
            }
        }
    };

    setNamedIcon("file.save", "document-save");
    setNamedIcon("app.save-as", "document-save");
    setNamedIcon("app.new", "document-new");
    setNamedIcon("app.open", "document-open");
    setNamedIcon("export.pdf", "document-export-pdf");
    setNamedIcon("export.png", "document-export-pdf");
    setNamedIcon("file.print", "document-print");
    setNamedIcon("edit.cut", "edit-cut");
    setNamedIcon("edit.copy", "edit-copy");
    setNamedIcon("edit.paste", "edit-paste");
    setNamedIcon("edit.undo-geometry", "edit-undo");
    setNamedIcon("edit.redo-geometry", "edit-redo");
    setNamedIcon("nav.back", "navigate-back");
    setNamedIcon("nav.forward", "navigate-forward");
    setNamedIcon("nav.next-annotated", "page-annotated-next");
    setNamedIcon("nav.prev-annotated", "page-annotated-next");
    setNamedIcon("page.add", "page-add");
    setNamedIcon("page.add-before", "page-add");
    setNamedIcon("page.add-end", "page-add");
    setNamedIcon("page.duplicate", "page-add");
    setNamedIcon("page.delete", "page-delete");
    setNamedIcon("page.delete-layer", "page-delete");
    setNamedIcon("view.fullscreen", "fullscreen");
    setNamedIcon("audio.record", "audio-record");
    setNamedIcon("audio.pause-playback", "audio-playback-pause");
    setNamedIcon("audio.seek-backwards", "audio-seek-backwards");
    setNamedIcon("audio.seek-forwards", "audio-seek-forwards");
    setNamedIcon("audio.stop-playback", "audio-playback-stop");
    setNamedIcon("audio.play-object", "object-play");
    setNamedIcon("view.paired-pages", "show-paired-pages");
    setNamedIcon("view.presentation", "presentation-mode");
    setNamedIcon("view.show-sidebar", "sidebar-show");
    setNamedIcon("app.settings", "toolbars-manage");
    setNamedIcon("view.customize-toolbar", "toolbars-customize");
    setNamedIcon("tool.hand", "hand");
    setNamedIcon("tool.pen", "tool-pencil");
    setNamedIcon("tool.laser-pointer-pen", "laser-pointer");
    setNamedIcon("tool.laser-pointer-highlighter", "laser-pointer");
    setNamedIcon("tool.setsquare", "setsquare");
    setNamedIcon("tool.compass", "compass");
    setNamedIcon("tool.eraser", "tool-eraser");
    setNamedIcon("tool.highlighter", "tool-highlighter");
    setNamedIcon("tool.text", "tool-text");
    setNamedIcon("tool.math-tex", "tool-math-tex");
    setNamedIcon("tool.select-pdf-text-linear", "select-pdf-text-ht");
    setNamedIcon("tool.select-pdf-text-rect", "select-pdf-text-area");
    setNamedIcon("edit.insert-image", "tool-image");
    setNamedIcon("tool.select", "select-rect");
    setNamedIcon("tool.select-region", "select-lasso");
    setNamedIcon("tool.select-multilayer-rect", "select-multilayer-rect");
    setNamedIcon("tool.select-multilayer-region", "select-multilayer-lasso");
    setNamedIcon("tool.select-object", "object-select");
    setNamedIcon("tool.vertical-space", "spacer");
    setNamedIcon("tool.draw-line", "draw-line");
    setNamedIcon("tool.draw-rectangle", "draw-rect");
    setNamedIcon("tool.draw-ellipse", "draw-ellipse");
    setNamedIcon("tool.draw-arrow", "draw-arrow");
    setNamedIcon("tool.draw-double-arrow", "draw-double-arrow");
    setNamedIcon("tool.draw-coordinate-system", "draw-coordinate-system");
    setNamedIcon("tool.draw-spline", "draw-spline");
    setNamedIcon("tool.draw-edge", "draw-line");
    setNamedIcon("tool.draw-circle", "draw-ellipse");
    setNamedIcon("tool.draw-arc", "draw-ellipse");
    setNamedIcon("tool.draw-construction-line", "draw-line");
    setNamedIcon("tool.draw-construction-circle", "draw-ellipse");
    setNamedIcon("tool.draw-polyline", "draw-line");
    setNamedIcon("tool.draw-shape-recognizer", "shape-recognizer");
    setNamedIcon("view.toggle-geometry-snap", "snapping-vertex");
    setNamedIcon("view.toggle-grid-snap", "snapping-grid");
    setNamedIcon("constraint.coincident", "object-select");
    setNamedIcon("constraint.horizontal", "draw-line");
    setNamedIcon("constraint.vertical", "draw-line");
    setNamedIcon("constraint.fixed-length", "draw-coordinate-system");
    setNamedIcon("constraint.edit-length", "draw-coordinate-system");
    setNamedIcon("constraint.radius", "draw-ellipse");
    setNamedIcon("constraint.parallel", "draw-line");
    setNamedIcon("constraint.perpendicular", "draw-coordinate-system");
    setNamedIcon("constraint.delete", "edit-delete");
    setNamedIcon("edit.select-font", "combo-selection");
    setNamedIcon("edit.delete-geometry", "edit-delete");
    setNamedIcon("edit.insert-vertex", "go-to");
    setNamedIcon("nav.goto-page", "go-to");
    setNamedIcon("view.show-toolbar", "toolbars-manage");
    setNamedIcon("view.show-menubar", "toolbars-customize");
    setNamedIcon("view.layout-horizontal", "orientation-landscape");
    setNamedIcon("view.layout-vertical", "orientation-portrait");
    setNamedIcon("view.layout-ltr", "navigate-forward");
    setNamedIcon("view.layout-rtl", "navigate-back");
    setNamedIcon("view.toggle-rotation-snap", "snapping-rotation");
    setNamedIcon("view.toggle-touch-drawing", "touch-drawing");
    setThemeIcon("view.layout-ttb", "go-top");
    setThemeIcon("view.layout-btt", "go-bottom");
    setNamedIcon("pen.size-very-fine", "thickness-finer");
    setNamedIcon("pen.size-fine", "thickness-fine");
    setNamedIcon("pen.size-medium", "thickness-medium");
    setNamedIcon("pen.size-thick", "thickness-thick");
    setNamedIcon("pen.size-very-thick", "thickness-thicker");
    setNamedIcon("highlighter.size-very-fine", "thickness-finer");
    setNamedIcon("highlighter.size-fine", "thickness-fine");
    setNamedIcon("highlighter.size-medium", "thickness-medium");
    setNamedIcon("highlighter.size-thick", "thickness-thick");
    setNamedIcon("highlighter.size-very-thick", "thickness-thicker");
    setNamedIcon("eraser.size-very-fine", "thickness-finer");
    setNamedIcon("eraser.size-fine", "thickness-fine");
    setNamedIcon("eraser.size-medium", "thickness-medium");
    setNamedIcon("eraser.size-thick", "thickness-thick");
    setNamedIcon("eraser.size-very-thick", "thickness-thicker");
    setNamedIcon("pen.line-solid", "line-style-plain-with-pen");
    setNamedIcon("pen.line-dash", "line-style-dash-with-pen");
    setNamedIcon("pen.line-dashdot", "line-style-dash-dot-with-pen");
    setNamedIcon("pen.line-dot", "line-style-dot-with-pen");
    setNamedIcon("pen.fill-toggle", "fill");
    setNamedIcon("highlighter.fill-toggle", "fill");
    setNamedIcon("tool.default-preset", "default");
    setNamedIcon("eraser.type-standard", "tool-eraser");
    setNamedIcon("eraser.type-whiteout", "transparent");
    setNamedIcon("eraser.type-delete-stroke", "edit-delete");
    setNamedIcon("page.format", "orientation-portrait");
    setNamedIcon("page.background", "transparent");
    setNamedIcon("layer.add-above", "combo-layer");
    setNamedIcon("layer.add-below", "combo-layer");
    setNamedIcon("layer.merge-down", "combo-layer");
    setNamedIcon("layer.rename", "combo-layer");
    setNamedIcon("layer.goto-prev", "combo-layer");
    setNamedIcon("layer.goto-next", "combo-layer");
    setNamedIcon("layer.goto-top", "combo-layer");
    setNamedIcon("edit.find", "select-pdf-text-ht");
    setNamedIcon("app.check-updates", "document-save");
    setNamedIcon("app.about-qt-shell", "default");
    setNamedIcon("view.paired-pages", "show-paired-pages");
    setNamedIcon("view.presentation", "presentation-mode");

    setThemeIcon("nav.first-page", "go-first");
    setThemeIcon("nav.prev-page", "go-previous");
    setThemeIcon("nav.next-page", "go-next");
    setThemeIcon("nav.last-page", "go-last");
    setThemeIcon("layer.goto-prev", "go-previous");
    setThemeIcon("layer.goto-next", "go-next");
    setThemeIcon("layer.goto-top", "go-top");
    setThemeIcon("view.zoom-in", "zoom-in");
    setThemeIcon("view.zoom-out", "zoom-out");
    setThemeIcon("view.fit-page", "zoom-fit-best");
    setThemeIcon("view.zoom-100", "zoom-original");
    setThemeIcon("edit.find", "edit-find");
    setThemeIcon("edit.delete", "edit-delete");
    setStandardIcon("view.zoom-reset", QStyle::SP_BrowserReload);
}
