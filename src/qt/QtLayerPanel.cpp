/*
 * VertexNote
 *
 * Qt layer panel implementation.
 */

#include "QtLayerPanel.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

#include "QtDocumentController.h"

QtLayerPanel::QtLayerPanel(QWidget* parent): QDockWidget(QStringLiteral("Layers"), parent) {
    setObjectName(QStringLiteral("vertexNoteQtLayerPanel"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    setMinimumWidth(96);
    setMaximumWidth(170);

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    this->layerList = new QListWidget(container);
    this->layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(this->layerList);

    auto* buttonBar = new QHBoxLayout();
    buttonBar->setSpacing(2);

    this->addButton = new QPushButton(QStringLiteral("+"), container);
    this->addButton->setToolTip(QStringLiteral("Add layer"));
    this->addButton->setFixedWidth(32);
    buttonBar->addWidget(this->addButton);

    this->removeButton = new QPushButton(QStringLiteral("\u2212"), container);
    this->removeButton->setToolTip(QStringLiteral("Remove layer"));
    this->removeButton->setFixedWidth(32);
    buttonBar->addWidget(this->removeButton);

    buttonBar->addStretch();

    this->upButton = new QPushButton(QStringLiteral("\u2191"), container);
    this->upButton->setToolTip(QStringLiteral("Move layer up"));
    this->upButton->setFixedWidth(32);
    buttonBar->addWidget(this->upButton);

    this->downButton = new QPushButton(QStringLiteral("\u2193"), container);
    this->downButton->setToolTip(QStringLiteral("Move layer down"));
    this->downButton->setFixedWidth(32);
    buttonBar->addWidget(this->downButton);

    layout->addLayout(buttonBar);
    container->setLayout(layout);
    setWidget(container);

    connect(this->layerList, &QListWidget::itemClicked, this, &QtLayerPanel::onItemClicked);
    connect(this->layerList, &QListWidget::itemChanged, this, &QtLayerPanel::onItemChanged);
    connect(this->addButton, &QPushButton::clicked, this, &QtLayerPanel::onAddLayer);
    connect(this->removeButton, &QPushButton::clicked, this, &QtLayerPanel::onRemoveLayer);
    connect(this->upButton, &QPushButton::clicked, this, &QtLayerPanel::onMoveUp);
    connect(this->downButton, &QPushButton::clicked, this, &QtLayerPanel::onMoveDown);
}

void QtLayerPanel::setDocumentController(QtDocumentController* ctrl) {
    this->controller = ctrl;
    refresh();
}

void QtLayerPanel::setCurrentPage(std::size_t pageIndex) {
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
        if (info.selected) {
            item->setSelected(true);
            this->layerList->setCurrentItem(item);
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
