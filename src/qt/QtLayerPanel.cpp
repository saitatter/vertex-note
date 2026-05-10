/*
 * VertexNote
 *
 * Qt layer panel implementation.
 */

#include "QtLayerPanel.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "config-paths.h"
#include "filesystem.h"
#include "QtDocumentController.h"

namespace {

auto bundledLayerIcon(std::string_view fileName) -> QIcon {
    const auto tryPath = [&](const fs::path& path) -> QIcon { return QIcon(QString::fromStdString(path.string())); };

    for (const auto& theme: {"iconsColor-dark", "iconsColor-light", "iconsLucide-light", "iconsLucide-dark"}) {
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

}  // namespace

QtLayerPanel::QtLayerPanel(QWidget* parent): QDockWidget(QStringLiteral("Layers"), parent) {
    setObjectName(QStringLiteral("vertexNoteQtLayerPanel"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    setMinimumWidth(100);
    setMaximumWidth(156);
    setTitleBarWidget(new QWidget(this));

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    this->layerList = new QListWidget(container);
    this->layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    this->layerList->setFrameShape(QFrame::NoFrame);
    this->layerList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->layerList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->layerList->setSpacing(1);
    layout->addWidget(this->layerList);

    auto* buttonBar = new QHBoxLayout();
    buttonBar->setSpacing(2);
    buttonBar->setContentsMargins(0, 0, 0, 0);

    this->addButton = new QToolButton(container);
    this->addButton->setToolTip(QStringLiteral("Add layer"));
    this->addButton->setAutoRaise(true);
    this->addButton->setIcon(bundledLayerIcon("xopp-page-add.svg"));
    this->addButton->setIconSize(QSize(18, 18));
    this->addButton->setFixedSize(24, 24);
    buttonBar->addWidget(this->addButton);

    this->removeButton = new QToolButton(container);
    this->removeButton->setToolTip(QStringLiteral("Remove layer"));
    this->removeButton->setAutoRaise(true);
    this->removeButton->setIcon(bundledLayerIcon("xopp-page-delete.svg"));
    this->removeButton->setIconSize(QSize(18, 18));
    this->removeButton->setFixedSize(24, 24);
    buttonBar->addWidget(this->removeButton);

    buttonBar->addStretch();

    this->upButton = new QToolButton(container);
    this->upButton->setToolTip(QStringLiteral("Move layer up"));
    this->upButton->setAutoRaise(true);
    this->upButton->setIcon(this->style()->standardIcon(QStyle::SP_ArrowUp));
    this->upButton->setIconSize(QSize(16, 16));
    this->upButton->setFixedSize(24, 24);
    buttonBar->addWidget(this->upButton);

    this->downButton = new QToolButton(container);
    this->downButton->setToolTip(QStringLiteral("Move layer down"));
    this->downButton->setAutoRaise(true);
    this->downButton->setIcon(this->style()->standardIcon(QStyle::SP_ArrowDown));
    this->downButton->setIconSize(QSize(16, 16));
    this->downButton->setFixedSize(24, 24);
    buttonBar->addWidget(this->downButton);

    layout->addLayout(buttonBar);
    container->setLayout(layout);
    setWidget(container);

    connect(this->layerList, &QListWidget::itemClicked, this, &QtLayerPanel::onItemClicked);
    connect(this->layerList, &QListWidget::itemChanged, this, &QtLayerPanel::onItemChanged);
    connect(this->addButton, &QToolButton::clicked, this, &QtLayerPanel::onAddLayer);
    connect(this->removeButton, &QToolButton::clicked, this, &QtLayerPanel::onRemoveLayer);
    connect(this->upButton, &QToolButton::clicked, this, &QtLayerPanel::onMoveUp);
    connect(this->downButton, &QToolButton::clicked, this, &QtLayerPanel::onMoveDown);
}

void QtLayerPanel::setDocumentController(QtDocumentController* ctrl) {
    this->controller = ctrl;
    refresh();
}

void QtLayerPanel::setCurrentPage(std::size_t pageIndex) {
    if (this->currentPageIndex == pageIndex && this->layerList->count() > 0) {
        return;
    }
    this->currentPageIndex = pageIndex;
    refresh();
}

void QtLayerPanel::refresh() {
    this->refreshing = true;
    this->layerList->clear();

    if (!this->controller) {
        this->refreshing = false;
        return;
    }

    const auto infos = this->controller->layerInfos(this->currentPageIndex);

    // Add layers in reverse order (topmost layer first in the UI)
    for (auto it = infos.rbegin(); it != infos.rend(); ++it) {
        const auto& info = *it;
        auto* item = new QListWidgetItem(this->layerList);
        item->setText(QString::fromStdString(info.name) +
                      QStringLiteral("  (%1)").arg(static_cast<int>(info.elementCount)));
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(info.index)));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(info.visible ? Qt::Checked : Qt::Unchecked);
        item->setSizeHint(QSize(0, 22));
        if (info.selected) {
            item->setSelected(true);
            this->layerList->setCurrentItem(item);
            this->layerList->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        }
    }

    this->refreshing = false;
}

void QtLayerPanel::onItemClicked(QListWidgetItem* item) {
    if (this->refreshing || !this->controller || !item) {
        return;
    }

    const auto layerIndex = static_cast<std::size_t>(item->data(Qt::UserRole).toULongLong());
    this->controller->selectLayer(this->currentPageIndex, layerIndex);
    Q_EMIT layerChanged();
}

void QtLayerPanel::onItemChanged(QListWidgetItem* item) {
    if (this->refreshing || !this->controller || !item) {
        return;
    }

    const auto layerIndex = static_cast<std::size_t>(item->data(Qt::UserRole).toULongLong());
    const bool visible = item->checkState() == Qt::Checked;
    this->controller->setLayerVisible(this->currentPageIndex, layerIndex, visible);
    Q_EMIT layerChanged();
}

void QtLayerPanel::onAddLayer() {
    if (!this->controller) {
        return;
    }
    this->controller->addLayer(this->currentPageIndex);
    refresh();
    Q_EMIT layerChanged();
}

void QtLayerPanel::onRemoveLayer() {
    if (!this->controller) {
        return;
    }

    auto* current = this->layerList->currentItem();
    if (!current) {
        return;
    }

    const auto layerIndex = static_cast<std::size_t>(current->data(Qt::UserRole).toULongLong());
    this->controller->removeLayer(this->currentPageIndex, layerIndex);
    refresh();
    Q_EMIT layerChanged();
}

void QtLayerPanel::onMoveUp() {
    if (!this->controller) {
        return;
    }

    auto* current = this->layerList->currentItem();
    if (!current) {
        return;
    }

    const auto layerIndex = static_cast<std::size_t>(current->data(Qt::UserRole).toULongLong());
    this->controller->moveLayerUp(this->currentPageIndex, layerIndex);
    refresh();
    Q_EMIT layerChanged();
}

void QtLayerPanel::onMoveDown() {
    if (!this->controller) {
        return;
    }

    auto* current = this->layerList->currentItem();
    if (!current) {
        return;
    }

    const auto layerIndex = static_cast<std::size_t>(current->data(Qt::UserRole).toULongLong());
    this->controller->moveLayerDown(this->currentPageIndex, layerIndex);
    refresh();
    Q_EMIT layerChanged();
}
