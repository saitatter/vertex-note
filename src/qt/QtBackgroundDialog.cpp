/*
 * VertexNote
 *
 * Qt page background dialog implementation.
 */

#include "QtBackgroundDialog.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

auto formatLabel(PageTypeFormat format) -> QString {
    switch (format) {
        case PageTypeFormat::Plain: return QStringLiteral("Plain");
        case PageTypeFormat::Ruled: return QStringLiteral("Ruled");
        case PageTypeFormat::Lined: return QStringLiteral("Lined");
        case PageTypeFormat::Graph: return QStringLiteral("Graph");
        case PageTypeFormat::IsoGraph: return QStringLiteral("Isometric Graph");
        case PageTypeFormat::Dotted: return QStringLiteral("Dotted");
        case PageTypeFormat::IsoDotted: return QStringLiteral("Isometric Dotted");
        case PageTypeFormat::Staves: return QStringLiteral("Music Staves");
        case PageTypeFormat::Pdf: return QStringLiteral("PDF (read-only)");
        case PageTypeFormat::Image: return QStringLiteral("Image (read-only)");
    }
    return QStringLiteral("Unknown");
}

constexpr PageTypeFormat SELECTABLE_FORMATS[] = {
        PageTypeFormat::Plain,  PageTypeFormat::Ruled,    PageTypeFormat::Lined,
        PageTypeFormat::Graph,  PageTypeFormat::IsoGraph, PageTypeFormat::Dotted,
        PageTypeFormat::IsoDotted, PageTypeFormat::Staves,
};

}  // namespace

QtBackgroundDialog::QtBackgroundDialog(Color currentColor, PageTypeFormat currentFormat, QWidget* parent):
        QDialog(parent), color(currentColor) {
    setWindowTitle(QStringLiteral("Page Background"));
    setMinimumWidth(320);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    // Background colour
    this->colorButton = new QPushButton(this);
    this->colorButton->setFixedSize(80, 28);
    updateColorPreview();
    connect(this->colorButton, &QPushButton::clicked, this, &QtBackgroundDialog::onChooseColor);
    form->addRow(QStringLiteral("Background colour:"), this->colorButton);

    // Pattern format
    this->formatCombo = new QComboBox(this);
    int selectedIndex = 0;
    for (std::size_t i = 0; i < std::size(SELECTABLE_FORMATS); ++i) {
        this->formatCombo->addItem(formatLabel(SELECTABLE_FORMATS[i]),
                                   static_cast<int>(SELECTABLE_FORMATS[i]));
        if (SELECTABLE_FORMATS[i] == currentFormat) {
            selectedIndex = static_cast<int>(i);
        }
    }
    this->formatCombo->setCurrentIndex(selectedIndex);
    form->addRow(QStringLiteral("Pattern:"), this->formatCombo);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

auto QtBackgroundDialog::selectedColor() const -> Color { return this->color; }

auto QtBackgroundDialog::selectedFormat() const -> PageTypeFormat {
    const int index = this->formatCombo->currentIndex();
    if (index >= 0 && index < static_cast<int>(std::size(SELECTABLE_FORMATS))) {
        return SELECTABLE_FORMATS[index];
    }
    return PageTypeFormat::Plain;
}

void QtBackgroundDialog::onChooseColor() {
    const QColor initial(static_cast<int>(this->color.red), static_cast<int>(this->color.green),
                         static_cast<int>(this->color.blue), static_cast<int>(this->color.alpha));
    const QColor chosen = QColorDialog::getColor(initial, this, QStringLiteral("Page Background Colour"));
    if (chosen.isValid()) {
        this->color = Color(static_cast<uint8_t>(chosen.red()), static_cast<uint8_t>(chosen.green()),
                            static_cast<uint8_t>(chosen.blue()), static_cast<uint8_t>(chosen.alpha()));
        updateColorPreview();
    }
}

void QtBackgroundDialog::updateColorPreview() {
    const QColor qc(static_cast<int>(this->color.red), static_cast<int>(this->color.green),
                    static_cast<int>(this->color.blue));
    this->colorButton->setStyleSheet(
            QStringLiteral("background-color: %1; border: 1px solid #888;").arg(qc.name()));
}
