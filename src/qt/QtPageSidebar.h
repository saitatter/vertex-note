/*
 * VertexNote
 *
 * Qt page sidebar with thumbnail previews.
 */

#pragma once

#include <QDockWidget>

#include "util/Color.h"

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
    void setRecolorOptions(bool enabled, Color light, Color dark);
    void setCurrentPage(std::size_t pageIndex);
    void refresh();

Q_SIGNALS:
    void pageSelected(std::size_t pageIndex);

private:
    void onItemClicked(QListWidgetItem* item);
    void syncCurrentSelection();
    auto renderThumbnail(std::size_t pageIndex) const -> QPixmap;

private:
    QListWidget* pageList = nullptr;
    QtDocumentController* controller = nullptr;
    vn::view::render::PageContentRenderer* contentRenderer = nullptr;
    std::size_t currentPageIndex = 0U;
    bool recolorEnabled = false;
    Color recolorLight{198, 208, 245, 255};
    Color recolorDark{48, 52, 70, 255};
};
