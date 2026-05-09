/*
 * VertexNote
 *
 * Qt layer panel dock widget.
 */

#pragma once

#include <QDockWidget>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QtDocumentController;

class QtLayerPanel: public QDockWidget {
    Q_OBJECT

public:
    explicit QtLayerPanel(QWidget* parent = nullptr);

    void setDocumentController(QtDocumentController* controller);
    void setCurrentPage(std::size_t pageIndex);
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
    QPushButton* addButton = nullptr;
    QPushButton* removeButton = nullptr;
    QPushButton* upButton = nullptr;
    QPushButton* downButton = nullptr;
    QtDocumentController* controller = nullptr;
    std::size_t currentPageIndex = 0U;
    bool refreshing = false;
};
