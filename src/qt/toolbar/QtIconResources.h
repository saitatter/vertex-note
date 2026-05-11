/*
 * VertexNote
 *
 * Qt icon lookup and command icon registry.
 */

#pragma once

#include <string>
#include <string_view>

#include <QIcon>

class QtMainWindow;
class QToolButton;
class QWidget;

void setQtIconAppearance(std::string theme, std::string tone);
[[nodiscard]] auto bundledQtIcon(std::string_view fileName) -> QIcon;
[[nodiscard]] auto bundledQtNamedIcon(std::string_view logicalName) -> QIcon;
[[nodiscard]] auto themeSymbolicIcon(std::string_view iconBaseName) -> QIcon;
[[nodiscard]] auto createStaticIconWidget(QWidget* parent, std::string_view iconFile, std::string_view tooltip)
        -> QToolButton*;
void applyQtCommandIcons(QtMainWindow& window);
