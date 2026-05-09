/*
 * VertexNote
 *
 * Qt page sidebar with thumbnail previews.
 */

#pragma once

#include <QDockWidget>

class QListWidget;
class QListWidgetItem;
class QtDocumentController;

namespace vn::view::render {
class PageContentRenderer;
}

class QtPageSidebar: public QDockWidget {
    Q_OBJECT

public:
    explicit QtPageSidebar(QWidget* parent = nullptr);

    void setDocumentController(QtDocumentController* controller);
    void setContentRenderer(vn::view::render::PageContentRenderer* renderer);
    void refresh();

Q_SIGNALS:
    void pageSelected(std::size_t pageIndex);

private:
    void onItemClicked(QListWidgetItem* item);
    auto renderThumbnail(std::size_t pageIndex) const -> QPixmap;

private:
    QListWidget* pageList = nullptr;
    QtDocumentController* controller = nullptr;
    vn::view::render::PageContentRenderer* contentRenderer = nullptr;
};
