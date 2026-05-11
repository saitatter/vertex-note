/*
 * VertexNote
 *
 * Qt app shell document and file workflows.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <filesystem>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <QApplication>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include "config-paths.h"
#include "config.h"
#include "control/latex/LatexGenerator.h"
#include "control/settings/LatexSettings.h"
#include "model/TexImage.h"
#include "util/PathUtil.h"
#include "util/raii/GObjectSPtr.h"
#include "vertexnote/update/GithubReleaseParser.h"
#include "vertexnote/update/ReleaseAssetSelector.h"
#include "vertexnote/update/ReleaseFetcher.h"
#include "vertexnote/update/VersionComparator.h"

namespace {

const std::vector<vn::ui::common::FileDialogFilter> SESSION_FILTERS = {
        {.label = "VertexNote Qt Session", .patterns = {"*.vnsession"}},
        {.label = "VertexNote Documents", .patterns = {"*.xopp", "*.xoj", "*.xopt", "*.pdf"}},
        {.label = "All Files", .patterns = {"*"}},
};

auto isSessionFile(const std::filesystem::path& path) -> bool { return path.extension() == ".vnsession"; }

auto joinFileDialogFilters(const std::vector<vn::ui::common::FileDialogFilter>& filters) -> QString {
    QStringList items;
    for (const auto& filter: filters) {
        QStringList patterns;
        for (const auto& pattern: filter.patterns) {
            patterns << QString::fromStdString(pattern);
        }
        items << QString::fromStdString(filter.label) + " (" + patterns.join(' ') + ")";
    }
    return items.join(";;");
}

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
    auto result = generator.asyncRun(texDir, texContents);
    if (auto* err = std::get_if<LatexGenerator::GenError>(&result)) {
        return err->message;
    }

    vn::util::GObjectSPtr<GSubprocess> process(std::get<GSubprocess*>(result), vn::util::adopt);
    GError* error = nullptr;
    char* stdoutBuffer = nullptr;
    const bool communicated =
            g_subprocess_communicate_utf8(process.get(), nullptr, nullptr, &stdoutBuffer, nullptr, &error);
    const std::string processOutput = stdoutBuffer ? stdoutBuffer : "";
    g_free(stdoutBuffer);

    if (!communicated) {
        const std::string message = error ? error->message : "VertexNote could not run the LaTeX generator.";
        if (error) {
            g_error_free(error);
        }
        return message;
    }

    const int exitStatus = g_subprocess_get_exit_status(process.get());
    if (exitStatus != 0) {
        if (!processOutput.empty()) {
            return processOutput;
        }
        return std::string("The LaTeX generator exited with an error.");
    }

    auto contents = Util::readString(texDir / "tex.pdf", false, std::ios::binary);
    if (!contents) {
        return std::string("VertexNote could not read the generated LaTeX PDF.");
    }

    auto image = std::make_unique<TexImage>();
    error = nullptr;
    const bool loaded = image->loadData(std::move(*contents), &error);
    if (error) {
        const std::string message = error->message;
        g_error_free(error);
        return message;
    }
    if (!loaded || !image->getPdf()) {
        return std::string("VertexNote could not load the generated LaTeX preview.");
    }

    image->setX(x);
    image->setY(y);
    image->setText(formula);
    return image;
}

}  // namespace
void QtAppShell::newSession() {
    this->session.newDocument();
    this->documentController.newBlankDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->newBlankDocument();
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    this->window.statusBar()->showMessage(QStringLiteral("Created a blank document"), 3000);
    updateWindowTitle();
}

void QtAppShell::rebuildRecentDocumentsMenu() {
    auto* menu = this->window.commandHost()->menuForPath("File>Recent Documents");
    menu->clear();

    const auto recentPaths = this->recentFiles.recentFiles();
    if (recentPaths.empty()) {
        auto* emptyAction = menu->addAction(QStringLiteral("No Recent Documents"));
        emptyAction->setEnabled(false);
        return;
    }

    for (std::size_t index = 0; index < recentPaths.size(); ++index) {
        const auto& path = recentPaths[index];
        const QString filename = QString::fromStdString(path.filename().string());
        const QString fullPath = QString::fromStdString(path.string());
        auto* action =
                menu->addAction(QStringLiteral("&%1 %2").arg(index + 1).arg(filename.isEmpty() ? fullPath : filename));
        action->setToolTip(fullPath);
        action->setStatusTip(fullPath);
        QObject::connect(action, &QAction::triggered, &this->window, [this, path]() { openPath(path, true); });
    }

    menu->addSeparator();
    auto* clearAction = menu->addAction(QStringLiteral("Clear Recent Documents"));
    QObject::connect(clearAction, &QAction::triggered, &this->window, [this]() {
        this->recentFiles.setRecentFiles({});
        rebuildRecentDocumentsMenu();
        savePersistentUiState();
        this->window.statusBar()->showMessage(QStringLiteral("Recent documents cleared"), 3000);
    });
}

auto QtAppShell::openPath(const std::filesystem::path& path, bool fromRecentDocuments) -> bool {
    if (!std::filesystem::exists(path)) {
        if (fromRecentDocuments) {
            auto recentPaths = this->recentFiles.recentFiles();
            recentPaths.erase(std::remove(recentPaths.begin(), recentPaths.end(), path), recentPaths.end());
            this->recentFiles.setRecentFiles(recentPaths);
            rebuildRecentDocumentsMenu();
            savePersistentUiState();
        }
        this->dialogs.showError("Open Failed",
                                "VertexNote could not find this recent document anymore. It was removed from the list.");
        return false;
    }

    if (isSessionFile(path)) {
        const auto sessionState = this->session.openFrom(path);
        if (!sessionState) {
            this->dialogs.showError("Open Failed", "VertexNote could not parse this Qt session file.");
            return false;
        }

        if (sessionState->linkedDocumentPath) {
            std::string error;
            if (!this->documentController.loadFrom(*sessionState->linkedDocumentPath, &error)) {
                this->dialogs.showError("Open Failed", error.empty() ? "VertexNote could not open the linked document."
                                                                     : error);
                return false;
            }
        } else {
            this->documentController.newBlankDocument();
        }

        this->suppressDirtyTracking = true;
        this->window.canvas()->setViewportState(sessionState->viewport.zoom, sessionState->viewport.scrollX,
                                                sessionState->viewport.scrollY);
        this->suppressDirtyTracking = false;
        this->recentFiles.addRecentFile(path);
        this->currentSettings.lastOpenPath = path.parent_path().string();
        rebuildRecentDocumentsMenu();
        savePersistentUiState();
        updateEditCommandStates();
        this->window.layerPanel()->refresh();
        this->window.pageSidebar()->refresh();
        syncFooterWidgets();
        this->window.statusBar()->showMessage(QString::fromStdString("Opened session " + path.filename().string()), 4000);
        updateWindowTitle();
        return true;
    }

    std::string error;
    if (!this->documentController.loadFrom(path, &error)) {
        this->dialogs.showError("Open Failed", error.empty() ? "VertexNote could not open this document." : error);
        return false;
    }

    this->session.newDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    this->recentFiles.addRecentFile(path);
    this->currentSettings.lastOpenPath = path.parent_path().string();
    rebuildRecentDocumentsMenu();
    savePersistentUiState();
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    this->window.statusBar()->showMessage(QString::fromStdString("Opened document " + path.filename().string()), 4000);
    updateWindowTitle();
    return true;
}

void QtAppShell::openSession() {
    const QString filePath = QFileDialog::getOpenFileName(&this->window, QStringLiteral("Open Document"),
                                                          dialogInitialDirectory(this->currentSettings.lastOpenPath),
                                                          joinFileDialogFilters(SESSION_FILTERS));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastOpenPath, filePath);
    openPath(std::filesystem::path(filePath.toStdWString()), false);
}

void QtAppShell::annotatePdf() {
    const QString filePath = QFileDialog::getOpenFileName(&this->window, QStringLiteral("Annotate PDF"),
                                                          dialogInitialDirectory(this->currentSettings.lastPdfPath),
                                                          QStringLiteral("PDF Files (*.pdf)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastPdfPath, filePath);

    const auto attachAnswer =
            QMessageBox::question(&this->window, QStringLiteral("Annotate PDF"),
                                  QStringLiteral("Attach the PDF data to the document when saving?"),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (attachAnswer == QMessageBox::Cancel) {
        return;
    }

    std::string error;
    const auto path = std::filesystem::path(filePath.toStdString());
    if (!this->documentController.loadPdfAsDocument(path, attachAnswer == QMessageBox::Yes, &error)) {
        this->dialogs.showError("Annotate PDF Failed",
                                error.empty() ? "VertexNote could not open this PDF." : error);
        return;
    }

    this->session.newDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    this->recentFiles.addRecentFile(path);
    rebuildRecentDocumentsMenu();
    savePersistentUiState();
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    this->window.statusBar()->showMessage(QStringLiteral("PDF opened for annotation"), 4000);
    updateWindowTitle();
}

void QtAppShell::saveSessionAs() {
    const auto suggestedPath = this->session.currentPath().value_or(std::filesystem::path("session.vnsession"));
    QString initialPath = QString::fromStdWString(suggestedPath.wstring());
    if (!this->currentSettings.lastSavePath.empty() && !suggestedPath.is_absolute()) {
        initialPath = QDir(dialogInitialDirectory(this->currentSettings.lastSavePath)).filePath(initialPath);
    }
    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Save Document"), initialPath,
                                                          joinFileDialogFilters(SESSION_FILTERS));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastSavePath, filePath);
    const auto path = std::filesystem::path(filePath.toStdWString());

    const QtSessionState sessionState{.viewport = this->window.canvas()->sessionViewportState(),
                                                  .linkedDocumentPath = this->documentController.sourcePath()};
    if (!this->session.saveAs(path, sessionState)) {
        this->dialogs.showError("Save Failed", "VertexNote could not save the Qt session file.");
        return;
    }

    this->recentFiles.addRecentFile(path);
    rebuildRecentDocumentsMenu();
    savePersistentUiState();
    this->window.statusBar()->showMessage(QString::fromStdString("Saved " + path.filename().string()), 4000);
    updateWindowTitle();
}

void QtAppShell::markSessionDirty() {
    if (!this->session.isDirty()) {
        this->session.markDirty(true);
        updateWindowTitle();
    }
}

void QtAppShell::checkForUpdates(bool silentWhenCurrent) {
    this->updates.showCheckingForUpdates();
    QPointer<QtMainWindow> windowGuard(&this->window);
    std::thread([this, windowGuard, silentWhenCurrent]() {
        struct UpdateResult {
            bool ok = false;
            bool available = false;
            vn::ui::common::UpdateReleaseSummary summary;
            std::string error;
        } result;

        try {
            const auto payload = vn::update::fetchLatestReleaseJson();
            const auto release = vn::update::parseGithubRelease(payload);
            if (!release) {
                result.error = "Could not parse GitHub release response.";
            } else {
                result.ok = true;
                result.available = vn::update::isUpdateAvailable(PROJECT_VERSION, release->tagName);
                const auto asset = vn::update::selectBestAsset(*release, vn::update::currentReleasePlatform());
                result.summary = {.version = release->tagName,
                                  .title = release->name.empty() ? release->tagName : release->name,
                                  .notes = release->body,
                                  .downloadUrl = asset ? asset->downloadUrl : release->htmlUrl};
            }
        } catch (const std::exception& ex) {
            result.error = ex.what();
        } catch (...) {
            result.error = "Unknown update check failure.";
        }

        if (!windowGuard) {
            return;
        }

        QMetaObject::invokeMethod(windowGuard.data(), [this, windowGuard, silentWhenCurrent, result = std::move(result)]() {
            if (!windowGuard) {
                return;
            }
            if (!result.ok) {
                if (!silentWhenCurrent) {
                    this->updates.showUpdateError(result.error.empty() ? "Could not check for updates." : result.error);
                }
                return;
            }
            if (result.available) {
                this->updates.showUpdateAvailable(result.summary);
            } else if (!silentWhenCurrent) {
                this->updates.showUpToDate(PROJECT_VERSION);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void QtAppShell::exportPdf() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Export PDF"),
                                                          dialogInitialDirectory(this->currentSettings.lastExportPath),
                                                          QStringLiteral("PDF Files (*.pdf)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastExportPath, filePath);

    auto* renderer = this->window.canvas()->contentRenderer();
    if (!renderer) {
        return;
    }

    QtDocumentExporter exp(renderer);
    std::string errorMsg;
    const auto& pages = this->documentController.snapshotPages();
    if (exp.exportPdf(filePath.toStdString(), pages, &errorMsg)) {
        this->window.statusBar()->showMessage(QStringLiteral("PDF exported successfully"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Export Failed"),
                             QString::fromStdString(errorMsg));
    }
}

void QtAppShell::exportPng() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Export PNG"),
                                                          dialogInitialDirectory(this->currentSettings.lastExportPath),
                                                          QStringLiteral("PNG Images (*.png)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastExportPath, filePath);

    auto* renderer = this->window.canvas()->contentRenderer();
    if (!renderer) {
        return;
    }

    // Export page 0 (current single-page focus)
    const auto& pages = this->documentController.snapshotPages();
    if (pages.empty()) {
        return;
    }

    QtDocumentExporter exp(renderer);
    std::string errorMsg;
    if (exp.exportPng(filePath.toStdString(), pages[0], 2.0, &errorMsg)) {
        this->window.statusBar()->showMessage(QStringLiteral("PNG exported successfully"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Export Failed"),
                             QString::fromStdString(errorMsg));
    }
}

void QtAppShell::saveDocument() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    // If there's an existing source path, save there; otherwise prompt
    auto existingPath = this->documentController.sourcePath();
    std::filesystem::path savePath;
    if (existingPath) {
        savePath = *existingPath;
    } else {
        const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Save Document"),
                                                              dialogInitialDirectory(this->currentSettings.lastSavePath),
                                                              QStringLiteral("VertexNote Files (*.xopp)"));
        if (filePath.isEmpty()) {
            return;
        }
        rememberDialogPath(this->currentSettings.lastSavePath, filePath);
        savePath = filePath.toStdString();
    }

    std::string errorMsg;
    if (this->documentController.saveDocument(savePath, &errorMsg)) {
        this->session.markDirty(false);
        this->recentFiles.addRecentFile(savePath);
        rebuildRecentDocumentsMenu();
        savePersistentUiState();
        updateWindowTitle();
        this->window.statusBar()->showMessage(QStringLiteral("Document saved"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Save Failed"), QString::fromStdString(errorMsg));
    }
}

void QtAppShell::printDocument() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    auto* renderer = this->window.canvas()->contentRenderer();
    if (!renderer) {
        return;
    }

    QtDocumentExporter exp(renderer);
    const auto& pages = this->documentController.snapshotPages();
    if (exp.printDocument(pages, &this->window)) {
        this->window.statusBar()->showMessage(QStringLiteral("Document printed"), 3000);
    }
}

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
