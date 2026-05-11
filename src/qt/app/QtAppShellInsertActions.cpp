/*
 * VertexNote
 *
 * Qt app shell find and insert workflows.
 */

#include "QtAppShell.h"

#include <filesystem>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include <QApplication>
#include <QByteArray>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QString>
#include <QVBoxLayout>

#include "config-paths.h"
#include "control/latex/LatexGenerator.h"
#include "control/settings/LatexSettings.h"
#include "filesystem.h"
#include "model/TexImage.h"
#include "util/PathUtil.h"

namespace {

auto defaultLatexTemplatePath() -> fs::path {
    return fs::path(PROJECT_SOURCE_DIR) / "resources" / "default_template.tex";
}

auto buildQtLatexSettings(const QtSettings& qtSettings) -> LatexSettings {
    LatexSettings settings;
    settings.autoCheckDependencies = qtSettings.latexAutoCheckDependencies;
    settings.defaultText = qtSettings.latexDefaultText;
    settings.globalTemplatePath =
            qtSettings.latexTemplatePath.empty() ? defaultLatexTemplatePath() : fs::path(qtSettings.latexTemplatePath);
    settings.genCmd = qtSettings.latexGenCmd;
    settings.sourceViewThemeId = qtSettings.latexSourceViewThemeId;
    settings.sourceViewAutoIndent = qtSettings.latexSourceViewAutoIndent;
    settings.sourceViewSyntaxHighlight = qtSettings.latexSourceViewSyntaxHighlight;
    settings.sourceViewShowLineNumbers = qtSettings.latexSourceViewShowLineNumbers;
    if (!qtSettings.latexEditorFont.empty()) {
        settings.editorFont = qtSettings.latexEditorFont;
    }
    settings.useCustomEditorFont = qtSettings.latexUseCustomEditorFont;
    settings.editorWordWrap = qtSettings.latexEditorWordWrap;
    settings.useExternalEditor = qtSettings.latexUseExternalEditor;
    settings.externalEditorAutoConfirm = qtSettings.latexExternalEditorAutoConfirm;
    settings.externalEditorCmd = qtSettings.latexExternalEditorCmd;
    settings.temporaryFileExt = qtSettings.latexTemporaryFileExt.empty() ? std::string("tex")
                                                                         : qtSettings.latexTemporaryFileExt;
    return settings;
}

auto loadLatexTemplate(const LatexSettings& settings) -> std::optional<std::string> {
    if (settings.globalTemplatePath.empty()) {
        return std::nullopt;
    }

    return Util::readString(settings.globalTemplatePath, false, std::ios::binary);
}

auto renderMathTex(const std::string& formula, const LatexSettings& settings, Color textColor, double x, double y)
        -> std::variant<std::unique_ptr<TexImage>, std::string> {
    const auto latexTemplate = loadLatexTemplate(settings);
    if (!latexTemplate) {
        return std::string("VertexNote could not load the LaTeX template file.");
    }

    auto texDir = Util::getTmpDirSubfolder("vertexnote-qt-tex");
    Util::ensureFolderExists(texDir);

    LatexGenerator generator(settings);
    const auto texContents = LatexGenerator::templateSub(formula, *latexTemplate, textColor);
    auto result = generator.run(texDir, texContents);
    if (auto* err = std::get_if<LatexGenerator::GenError>(&result)) {
        return err->message;
    }

    const auto& output = std::get<LatexGenerator::GenOutput>(result);
    if (output.exitStatus != 0) {
        if (!output.output.empty()) {
            return output.output;
        }
        return std::string("The LaTeX generator exited with an error.");
    }

    auto contents = Util::readString(texDir / "tex.pdf", false, std::ios::binary);
    if (!contents) {
        return std::string("VertexNote could not read the generated LaTeX PDF.");
    }

    auto image = std::make_unique<TexImage>();
    std::string loadError;
    const bool loaded = image->loadData(std::move(*contents), &loadError);
    if (!loaded || !image->getPdf()) {
        if (!loadError.empty()) {
            return loadError;
        }
        return std::string("VertexNote could not load the generated LaTeX preview.");
    }

    image->setX(x);
    image->setY(y);
    image->setText(formula);
    return image;
}

}  // namespace
void QtAppShell::findText() {
    bool ok = false;
    const QString searchTerm = QInputDialog::getText(&this->window, QStringLiteral("Find Text"),
                                                     QStringLiteral("Search for:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || searchTerm.isEmpty()) {
        return;
    }

    const auto results = this->documentController.findTextInDocument(searchTerm.toStdString());
    if (results.empty()) {
        QMessageBox::information(&this->window, QStringLiteral("Find Text"),
                                 QStringLiteral("No matches found for \"%1\".").arg(searchTerm));
        return;
    }

    // Show summary and scroll to first result
    const auto& first = results.front();
    QString msg = QStringLiteral("Found %1 match(es). First on page %2.")
                          .arg(results.size())
                          .arg(first.pageIndex + 1);
    this->window.statusBar()->showMessage(msg, 5000);

    // Scroll to the page of the first result
    const auto& pages = this->documentController.snapshotPages();
    double y = 0.0;
    constexpr double PAGE_GAP = 20.0;
    for (std::size_t i = 0; i < first.pageIndex && i < pages.size(); ++i) {
        y += pages[i].height + PAGE_GAP;
    }
    this->window.canvas()->setViewportState(this->window.canvas()->sessionViewportState().zoom, 0.0, y);
    this->window.canvas()->update();
}

void QtAppShell::insertImage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath =
            QFileDialog::getOpenFileName(&this->window, QStringLiteral("Insert Image"),
                                         dialogInitialDirectory(this->currentSettings.lastImagePath),
                                         QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastImagePath, filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(&this->window, QStringLiteral("Insert Image"), QStringLiteral("Could not read the image file."));
        return;
    }

    const QByteArray imageData = file.readAll();
    file.close();

    // Insert on page 0, layer 0 at a default position
    this->documentController.insertImage(0, 100.0, 100.0, std::string(imageData.constData(), imageData.size()),
                                         200.0, 200.0);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Image inserted"), 3000);
}

void QtAppShell::insertMathTex() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const auto settings = buildQtLatexSettings(this->currentSettings);
    if (!fs::is_regular_file(settings.globalTemplatePath)) {
        QMessageBox::warning(&this->window, QStringLiteral("Math TeX"),
                             QStringLiteral("VertexNote could not find the LaTeX template file."));
        return;
    }

    QDialog dialog(&this->window);
    dialog.setWindowTitle(QStringLiteral("Insert Math TeX"));
    dialog.resize(560, 360);

    auto* layout = new QVBoxLayout(&dialog);
    auto* hint = new QLabel(
            QStringLiteral("Enter LaTeX to render into the current page. VertexNote will compile it with the shared LaTeX pipeline."),
            &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* editor = new QPlainTextEdit(&dialog);
    editor->setObjectName(QStringLiteral("vertexNoteQtMathTexEditor"));
    editor->setPlainText(QString::fromStdString(settings.defaultText));
    editor->setLineWrapMode(settings.editorWordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    layout->addWidget(editor, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString formulaText = editor->toPlainText().trimmed();
    if (formulaText.isEmpty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Math TeX insertion canceled: empty formula"), 3000);
        return;
    }

    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto& pages = this->documentController.snapshotPages();
    if (pageIndex >= pages.size()) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto renderResult = renderMathTex(formulaText.toStdString(), settings, this->window.canvas()->toolState().penColor,
                                      pages[pageIndex].width * 0.5, pages[pageIndex].height * 0.5);
    QApplication::restoreOverrideCursor();

    if (const auto* error = std::get_if<std::string>(&renderResult)) {
        QMessageBox::warning(&this->window, QStringLiteral("Math TeX"),
                             QStringLiteral("VertexNote could not render this LaTeX formula.\n\n%1")
                                     .arg(QString::fromStdString(*error)));
        return;
    }

    auto* texImage = std::get_if<std::unique_ptr<TexImage>>(&renderResult);
    if (!texImage || !*texImage) {
        QMessageBox::warning(&this->window, QStringLiteral("Math TeX"),
                             QStringLiteral("VertexNote could not create the rendered TeX image."));
        return;
    }

    ElementPtr element(texImage->release());
    if (!this->documentController.insertElement(pageIndex, std::move(element), "Insert LaTeX")) {
        QMessageBox::warning(&this->window, QStringLiteral("Math TeX"),
                             QStringLiteral("VertexNote could not insert the rendered TeX image into the document."));
        return;
    }

    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    this->window.layerPanel()->refresh();
    syncFooterWidgets();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("LaTeX formula inserted"), 3000);
}
