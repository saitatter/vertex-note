/*
 * VertexNote
 *
 * Qt layer panel dock widget.
 */

#pragma once

#include <string>

#include <QDockWidget>

class QListWidget;
class QListWidgetItem;
class QToolButton;
class QtDocumentController;

class QtLayerPanel: public QDockWidget {
    Q_OBJECT

public:
    explicit QtLayerPanel(QWidget* parent = nullptr);

    void setDocumentController(QtDocumentController* controller);
    void setCurrentPage(std::size_t pageIndex);
    void setIconAppearance(std::string iconTheme, std::string iconTone);
    void refresh();

Q_SIGNALS:
    void layerChanged();

private:
    void onItemClicked(QListWidgetItem* item);
    void onItemChanged(QListWidgetItem* item);
    void onAddLayer();
    void onRemoveLayer();
    void onMoveUp();
    void onMoveDown();

private:
    QListWidget* layerList = nullptr;
    QToolButton* addButton = nullptr;
    QToolButton* removeButton = nullptr;
    QToolButton* upButton = nullptr;
    QToolButton* downButton = nullptr;
    QtDocumentController* controller = nullptr;
    std::size_t currentPageIndex = 0U;
    std::string iconTheme = "color";
    std::string iconTone = "light";
    bool refreshing = false;
};
