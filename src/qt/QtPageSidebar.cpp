/*
 * VertexNote
 *
 * Qt page sidebar implementation with rendered thumbnails.
 */

#include "QtPageSidebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

#include "QtDocumentController.h"
#include "QtPageContentRenderer.h"
#include "view/render/QtPainterRenderContext.h"

namespace {
constexpr int THUMB_WIDTH = 160;
constexpr int THUMB_HEIGHT = 220;
}  // namespace

QtPageSidebar::QtPageSidebar(QWidget* parent): QDockWidget(QStringLiteral("Pages"), parent) {
    setObjectName(QStringLiteral("vertexNoteQtPageSidebar"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    this->pageList = new QListWidget(container);
    this->pageList->setViewMode(QListView::ListMode);
    this->pageList->setIconSize(QSize(THUMB_WIDTH, THUMB_HEIGHT));
    this->pageList->setSpacing(4);
    this->pageList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(this->pageList);

    container->setLayout(layout);
    setWidget(container);

    connect(this->pageList, &QListWidget::itemClicked, this, &QtPageSidebar::onItemClicked);
}

void QtPageSidebar::setDocumentController(QtDocumentController* ctrl) {
    this->controller = ctrl;
    refresh();
}

void QtPageSidebar::setContentRenderer(vn::view::render::PageContentRenderer* renderer) {
    this->contentRenderer = renderer;
}

void QtPageSidebar::refresh() {
    this->pageList->clear();

    if (!this->controller) {
        return;
    }

    const auto& pages = this->controller->snapshotPages();
    for (std::size_t i = 0; i < pages.size(); ++i) {
        auto* item = new QListWidgetItem(this->pageList);
        item->setText(QStringLiteral("Page %1").arg(static_cast<int>(i + 1)));
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(i)));
        item->setIcon(QIcon(renderThumbnail(i)));
        item->setSizeHint(QSize(THUMB_WIDTH + 16, THUMB_HEIGHT + 28));
    }
}

void QtPageSidebar::onItemClicked(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    const auto pageIndex = static_cast<std::size_t>(item->data(Qt::UserRole).toULongLong());
    Q_EMIT pageSelected(pageIndex);
}

auto QtPageSidebar::renderThumbnail(std::size_t pageIndex) const -> QPixmap {
    if (!this->controller || !this->contentRenderer) {
        QPixmap blank(THUMB_WIDTH, THUMB_HEIGHT);
        blank.fill(Qt::white);
        return blank;
    }

    const auto& pages = this->controller->snapshotPages();
    if (pageIndex >= pages.size()) {
        QPixmap blank(THUMB_WIDTH, THUMB_HEIGHT);
        blank.fill(Qt::white);
        return blank;
    }

    const auto& page = pages[pageIndex];
    const double pageWidth = std::max(page.width, 1.0);
    const double pageHeight = std::max(page.height, 1.0);

    // Scale to fit thumbnail
    const double scaleX = static_cast<double>(THUMB_WIDTH) / pageWidth;
    const double scaleY = static_cast<double>(THUMB_HEIGHT) / pageHeight;
    const double scale = std::min(scaleX, scaleY);

    const int pixWidth = static_cast<int>(std::ceil(pageWidth * scale));
    const int pixHeight = static_cast<int>(std::ceil(pageHeight * scale));

    QPixmap pixmap(pixWidth, pixHeight);
    pixmap.fill(Qt::white);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(scale, scale);

    vn::view::render::QtPainterRenderContext renderContext(&painter, scale);
    const vn::view::render::RenderRect renderRect{
            .x = 0.0,
            .y = 0.0,
            .width = pageWidth,
            .height = pageHeight,
    };
    this->contentRenderer->drawPage(page, renderRect, renderContext);

    painter.end();

    // Draw a thin border
    QPainter borderPainter(&pixmap);
    borderPainter.setPen(QPen(QColor(180, 180, 180), 1.0));
    borderPainter.setBrush(Qt::NoBrush);
    borderPainter.drawRect(0, 0, pixWidth - 1, pixHeight - 1);

    return pixmap;
}
