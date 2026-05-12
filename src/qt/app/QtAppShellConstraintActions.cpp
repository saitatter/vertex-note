/*
 * VertexNote
 *
 * Qt app shell geometry constraint actions.
 */

#include "QtAppShell.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QStatusBar>
#include <QString>
#include <QVBoxLayout>

void QtAppShell::applyConstraint(vn::geom::ConstraintKind kind) {
    if (!this->documentController.hasDocument()) {
        return;
    }
    if (this->documentController.applyConstraint(kind)) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Constraint applied"), 3000);
    } else {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot apply constraint — check selection"), 3000);
    }
}

void QtAppShell::deleteConstraints() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    (void)this->documentController.deleteSelectedConstraints();
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Constraints deleted"), 3000);
}

void QtAppShell::editFixedLengthConstraint() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto existing = this->documentController.selectedFixedLengthConstraint();
    if (!existing) {
        this->window.statusBar()->showMessage(
                QStringLiteral("No fixed-length or radius constraint on selection"), 3000);
        return;
    }

    bool ok = false;
    const double newValue = QInputDialog::getDouble(&this->window, QStringLiteral("Edit Constraint Value"),
                                                    QStringLiteral("Value:"), existing->value, 0.01, 100000.0, 2, &ok);
    if (!ok) {
        return;
    }

    (void)this->documentController.updateFixedLengthConstraint(newValue);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Constraint value updated"), 3000);
}

void QtAppShell::translateSelectedVertices() {
    if (!this->documentController.hasDocument() || this->documentController.selectedVertexIds().empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Select one or more vertices first"), 3000);
        return;
    }

    QDialog dialog(&this->window);
    dialog.setWindowTitle(QStringLiteral("Translate Selected Vertices"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* dxSpin = new QDoubleSpinBox(&dialog);
    dxSpin->setRange(-100000.0, 100000.0);
    dxSpin->setDecimals(2);
    dxSpin->setSingleStep(1.0);
    dxSpin->setSuffix(QStringLiteral(" pt"));
    form->addRow(QStringLiteral("Delta X:"), dxSpin);

    auto* dySpin = new QDoubleSpinBox(&dialog);
    dySpin->setRange(-100000.0, 100000.0);
    dySpin->setDecimals(2);
    dySpin->setSingleStep(1.0);
    dySpin->setSuffix(QStringLiteral(" pt"));
    form->addRow(QStringLiteral("Delta Y:"), dySpin);

    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (this->documentController.translateSelectedVertices(dxSpin->value(), dySpin->value())) {
        this->window.canvas()->update();
        markSessionDirty();
        updateEditCommandStates();
        this->window.statusBar()->showMessage(QStringLiteral("Translated selected vertices"), 3000);
    } else {
        this->window.statusBar()->showMessage(QStringLiteral("No vertex translation applied"), 3000);
    }
}
