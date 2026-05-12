/*
 * VertexNote
 *
 * Qt page background dialog for changing page colour and pattern.
 */

#pragma once

#include <QDialog>

#include "model/PageType.h"
#include "util/Color.h"

class QComboBox;
class QPushButton;

class QtBackgroundDialog: public QDialog {
    Q_OBJECT

public:
    explicit QtBackgroundDialog(Color currentColor, PageTypeFormat currentFormat, QWidget* parent = nullptr);

    [[nodiscard]] auto selectedColor() const -> Color;
    [[nodiscard]] auto selectedFormat() const -> PageTypeFormat;

private:
    void onChooseColor();
    void updateColorPreview();

private:
    Color color;
    QComboBox* formatCombo = nullptr;
    QPushButton* colorButton = nullptr;
};
