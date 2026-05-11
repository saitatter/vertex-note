/*
 * VertexNote
 *
 * Qt page sidebar implementation with rendered thumbnails.
 */

#include "QtPageSidebar.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QVBoxLayout>

#include "QtDocumentController.h"
#include "QtPageContentRenderer.h"
#include "view/render/QtPainterRenderContext.h"

namespace {
constexpr int THUMB_WIDTH = 64;
constexpr int THUMB_HEIGHT = 92;
constexpr int THUMB_ITEM_WIDTH = 72;
constexpr int SIDEBAR_NUMBERING_NONE = 0;
constexpr int SIDEBAR_NUMBERING_BELOW = 1;
constexpr int SIDEBAR_NUMBERING_CIRCLE = 2;
constexpr int SIDEBAR_NUMBERING_SQUARE = 3;
constexpr int SCROLLBAR_HIDE_HORIZONTAL = 1 << 1;
constexpr int SCROLLBAR_HIDE_VERTICAL = 1 << 2;

auto recolorDifference(Color light, Color dark) -> QColor {
    return QColor(std::abs(static_cast<int>(dark.red) - static_cast<int>(light.red)),
                  std::abs(static_cast<int>(dark.green) - static_cast<int>(light.green)),
                  std::abs(static_cast<int>(dark.blue) - static_cast<int>(light.blue)));
}

auto recolorOffset(Color light, Color dark) -> QColor {
    return QColor(std::min(light.red, dark.red), std::min(light.green, dark.green), std::min(light.blue, dark.blue));
}

auto recolorReference(Color light, Color dark) -> QColor {
    return QColor(light.red < dark.red ? 255 : 0, light.green < dark.green ? 255 : 0, light.blue < dark.blue ? 255 : 0);
}
}  // namespace

QtPageSidebar::QtPageSidebar(QWidget* parent): QDockWidget(QStringLiteral("Pages"), parent) {
    setObjectName(QStringLiteral("vertexNoteQtPageSidebar"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    setMinimumWidth(76);
    setMaximumWidth(600);
    setTitleBarWidget(new QWidget(this));

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(1);

    this->pageList = new QListWidget(container);
    this->pageList->setViewMode(QListView::IconMode);
    this->pageList->setFlow(QListView::TopToBottom);
    this->pageList->setWrapping(false);
    this->pageList->setMovement(QListView::Static);
    this->pageList->setIconSize(QSize(THUMB_WIDTH, THUMB_HEIGHT));
    this->pageList->setResizeMode(QListView::Adjust);
    this->pageList->setSpacing(2);
    this->pageList->setSelectionMode(QAbstractItemView::SingleSelection);
    this->pageList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->pageList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->pageList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->pageList->setFrameShape(QFrame::NoFrame);
    this->pageList->setWordWrap(true);
    this->pageList->setGridSize(QSize(THUMB_ITEM_WIDTH, THUMB_HEIGHT + 20));
    this->pageList->setUniformItemSizes(true);
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

void QtPageSidebar::setRecolorOptions(bool enabled, Color light, Color dark) {
    this->recolorEnabled = enabled;
    this->recolorLight = light;
    this->recolorDark = dark;
    refresh();
}

void QtPageSidebar::setPreferredSidebarWidth(int width) {
    this->preferredSidebarWidth = std::clamp(width, 76, 600);
    setMinimumWidth(76);
    setMaximumWidth(600);
    resize(this->preferredSidebarWidth, height());
}

void QtPageSidebar::setDisplayOptions(int numbering, int scrollbarHide, bool scrollbarLeft, bool disableFadeout) {
    this->numberingStyle = std::clamp(numbering, SIDEBAR_NUMBERING_NONE, SIDEBAR_NUMBERING_SQUARE);
    this->scrollbarHideType = scrollbarHide;
    this->scrollbarOnLeft = scrollbarLeft;
    this->disableScrollbarFadeout = disableFadeout;

    const bool hideHorizontal = (this->scrollbarHideType & SCROLLBAR_HIDE_HORIZONTAL) != 0;
    const bool hideVertical = (this->scrollbarHideType & SCROLLBAR_HIDE_VERTICAL) != 0;
    this->pageList->setHorizontalScrollBarPolicy(hideHorizontal ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    this->pageList->setVerticalScrollBarPolicy(hideVertical ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    this->pageList->setLayoutDirection(this->scrollbarOnLeft ? Qt::RightToLeft : Qt::LeftToRight);
    this->pageList->viewport()->setLayoutDirection(Qt::LeftToRight);
    this->pageList->verticalScrollBar()->setProperty("vertexDisableFadeout", this->disableScrollbarFadeout);
    refresh();
}

void QtPageSidebar::setCurrentPage(std::size_t pageIndex) {
    if (this->currentPageIndex == pageIndex && this->pageList->count() > 0) {
        syncCurrentSelection();
        return;
    }
    this->currentPageIndex = pageIndex;
    if (static_cast<int>(pageIndex) < this->pageList->count()) {
        syncCurrentSelection();
        return;
    }
    refresh();
}

void QtPageSidebar::refresh() {
    this->pageList->clear();

    if (!this->controller) {
        return;
    }

    const auto& pages = this->controller->snapshotPages();
    for (std::size_t i = 0; i < pages.size(); ++i) {
        auto* item = new QListWidgetItem(this->pageList);
        item->setText(this->numberingStyle == SIDEBAR_NUMBERING_BELOW
                              ? QStringLiteral("%1").arg(static_cast<int>(i + 1))
                              : QString());
        item->setTextAlignment(Qt::AlignHCenter);
        item->setForeground(QBrush(QColor(215, 64, 64)));
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(i)));
        item->setIcon(QIcon(renderThumbnail(i)));
        item->setSizeHint(QSize(THUMB_ITEM_WIDTH, THUMB_HEIGHT +
                                                       (this->numberingStyle == SIDEBAR_NUMBERING_BELOW ? 22 : 8)));
    }
    syncCurrentSelection();
}

void QtPageSidebar::onItemClicked(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    const auto pageIndex = static_cast<std::size_t>(item->data(Qt::UserRole).toULongLong());
    Q_EMIT pageSelected(pageIndex);
}

void QtPageSidebar::syncCurrentSelection() {
    if (!this->pageList || this->pageList->count() == 0) {
        return;
    }

    const int itemIndex = std::clamp<int>(static_cast<int>(this->currentPageIndex), 0, this->pageList->count() - 1);
    if (auto* item = this->pageList->item(itemIndex)) {
        this->pageList->setCurrentItem(item);
        item->setSelected(true);
        this->pageList->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    }
}

auto QtPageSidebar::renderThumbnail(std::size_t pageIndex) const -> QPixmap {
    if (!this->controller || !this->contentRenderer) {
        QPixmap blank(THUMB_WIDTH, THUMB_HEIGHT);
        blank.fill(Qt::white);
        return blank;
    }

    this->controller->preparePdfRasterCache({pageIndex});
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

    if (this->recolorEnabled) {
        painter.resetTransform();
        painter.setCompositionMode(QPainter::CompositionMode_Difference);
        painter.fillRect(pixmap.rect(), recolorReference(this->recolorLight, this->recolorDark));
        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        painter.fillRect(pixmap.rect(), recolorDifference(this->recolorLight, this->recolorDark));
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
        painter.fillRect(pixmap.rect(), recolorOffset(this->recolorLight, this->recolorDark));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    painter.end();

    // Draw a thin border
    QPainter borderPainter(&pixmap);
    borderPainter.setPen(QPen(QColor(180, 180, 180), 1.0));
    borderPainter.setBrush(Qt::NoBrush);
    borderPainter.drawRect(0, 0, pixWidth - 1, pixHeight - 1);
    if (this->numberingStyle == SIDEBAR_NUMBERING_CIRCLE || this->numberingStyle == SIDEBAR_NUMBERING_SQUARE) {
        const QString pageNumber = QString::number(static_cast<int>(pageIndex + 1));
        const int badgeSize = std::max(18, std::min(28, pixWidth / 3));
        const QRect badgeRect(pixWidth - badgeSize - 4, pixHeight - badgeSize - 4, badgeSize, badgeSize);
        borderPainter.setRenderHint(QPainter::Antialiasing, true);
        borderPainter.setPen(Qt::NoPen);
        borderPainter.setBrush(QColor(215, 64, 64, 230));
        if (this->numberingStyle == SIDEBAR_NUMBERING_CIRCLE) {
            borderPainter.drawEllipse(badgeRect);
        } else {
            borderPainter.drawRoundedRect(badgeRect, 2, 2);
        }
        borderPainter.setPen(QColor(255, 255, 255));
        borderPainter.drawText(badgeRect, Qt::AlignCenter, pageNumber);
    }

    return pixmap;
}
