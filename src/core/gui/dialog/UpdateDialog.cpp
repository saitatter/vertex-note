#include "UpdateDialog.h"

#include <gio/gio.h>

#include <optional>
#include <string>
#include <utility>

#include "util/XojMsgBox.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"
#include "vertexnote/update/GithubReleaseParser.h"
#include "vertexnote/update/ReleaseInfo.h"
#include "vertexnote/update/VersionComparator.h"

#include "config.h"

namespace xoj::popup {
namespace {

constexpr auto RELEASES_URL = "https://github.com/saitatter/vertex-note/releases";
constexpr auto GITHUB_LATEST_RELEASE_API = "https://api.github.com/repos/saitatter/vertex-note/releases/latest";

class UpdateDialogController {
public:
    explicit UpdateDialogController(GtkWindow* parent):
            dialog(GTK_DIALOG(gtk_dialog_new_with_buttons(_("VertexNote Updates"), parent,
                                                          static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL |
                                                                                      GTK_DIALOG_DESTROY_WITH_PARENT),
                                                          _("_Close"), GTK_RESPONSE_CLOSE, nullptr))),
            statusLabel(GTK_LABEL(gtk_label_new(_("Ready to check for updates.")))),
            latestLabel(GTK_LABEL(gtk_label_new("-"))),
            textView(GTK_TEXT_VIEW(gtk_text_view_new())) {
        gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 520);

        auto* content = gtk_dialog_get_content_area(dialog);
        auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_set_border_width(GTK_CONTAINER(box), 12);
        gtk_box_append(GTK_BOX(content), box);

        auto* heading = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(heading), _("<b>VertexNote release channel</b>"));
        gtk_widget_set_halign(heading, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(box), heading);

        auto* grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
        gtk_box_append(GTK_BOX(box), grid);

        attachRow(GTK_GRID(grid), 0, _("Current version"), PROJECT_VERSION);
        attachRow(GTK_GRID(grid), 1, _("Latest release"), latestLabel);
        attachRow(GTK_GRID(grid), 2, _("Status"), statusLabel);

        gtk_text_view_set_editable(textView, false);
        gtk_text_view_set_wrap_mode(textView, GTK_WRAP_WORD_CHAR);
        auto* scroller = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_widget_set_vexpand(scroller, true);
        gtk_container_add(GTK_CONTAINER(scroller), GTK_WIDGET(textView));
        gtk_box_append(GTK_BOX(box), scroller);

        refreshButton = gtk_dialog_add_button(dialog, _("Check Now"), GTK_RESPONSE_APPLY);
        releaseButton = gtk_dialog_add_button(dialog, _("Open Releases"), GTK_RESPONSE_HELP);
        downloadButton = gtk_dialog_add_button(dialog, _("Download Update"), GTK_RESPONSE_ACCEPT);
        gtk_widget_set_sensitive(downloadButton, false);

        g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* dialog, int response, gpointer data) {
                             auto* self = static_cast<UpdateDialogController*>(data);
                             if (response == GTK_RESPONSE_APPLY) {
                                 self->startCheck();
                             } else if (response == GTK_RESPONSE_HELP) {
                                 XojMsgBox::openURL(GTK_WINDOW(dialog), self->releaseUrl().c_str());
                             } else if (response == GTK_RESPONSE_ACCEPT) {
                                 XojMsgBox::openURL(GTK_WINDOW(dialog), self->downloadUrl().c_str());
                             } else {
                                 gtk_window_close(GTK_WINDOW(dialog));
                             }
                         }),
                         this);
        g_signal_connect(dialog, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
                             auto* self = static_cast<UpdateDialogController*>(data);
                             self->destroyed = true;
                             if (!self->requestInFlight) {
                                 delete self;
                             }
                         }),
                         this);

        gtk_widget_show_all(GTK_WIDGET(dialog));
        startCheck();
    }

private:
    static void attachRow(GtkGrid* grid, int row, const char* name, const char* value) {
        auto* label = gtk_label_new(name);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(grid, label, 0, row, 1, 1);

        auto* valueLabel = gtk_label_new(value);
        gtk_widget_set_halign(valueLabel, GTK_ALIGN_START);
        gtk_grid_attach(grid, valueLabel, 1, row, 1, 1);
    }

    static void attachRow(GtkGrid* grid, int row, const char* name, GtkLabel* valueLabel) {
        auto* label = gtk_label_new(name);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(grid, label, 0, row, 1, 1);

        gtk_widget_set_halign(GTK_WIDGET(valueLabel), GTK_ALIGN_START);
        gtk_grid_attach(grid, GTK_WIDGET(valueLabel), 1, row, 1, 1);
    }

    auto releaseUrl() const -> const std::string& {
        static const std::string fallback = RELEASES_URL;
        return latestRelease && !latestRelease->htmlUrl.empty() ? latestRelease->htmlUrl : fallback;
    }

    auto downloadUrl() const -> const std::string& {
        static const std::string fallback = RELEASES_URL;
        if (!latestRelease || latestRelease->assets.empty()) {
            return fallback;
        }
        return latestRelease->assets.front().downloadUrl;
    }

    void setBodyText(const std::string& text) {
        auto* buffer = gtk_text_view_get_buffer(textView);
        gtk_text_buffer_set_text(buffer, text.c_str(), static_cast<gint>(text.size()));
    }

    void setCheckingState() {
        gtk_label_set_text(statusLabel, _("Checking GitHub releases..."));
        setBodyText(_("Fetching release notes from GitHub. If this takes too long, use Open Releases."));
        gtk_widget_set_sensitive(refreshButton, false);
        gtk_widget_set_sensitive(downloadButton, false);
    }

    void setErrorState(const std::string& message) {
        gtk_label_set_text(statusLabel, _("Could not check automatically."));
        setBodyText(message + "\n\n" + _("You can still open the VertexNote Releases page from this dialog."));
        gtk_widget_set_sensitive(refreshButton, true);
        gtk_widget_set_sensitive(downloadButton, false);
    }

    void setReleaseState(vn::update::ReleaseInfo release) {
        latestRelease = std::move(release);
        gtk_label_set_text(latestLabel, latestRelease->tagName.c_str());

        const auto updateAvailable = vn::update::isUpdateAvailable(PROJECT_VERSION, latestRelease->tagName);
        gtk_label_set_text(statusLabel, updateAvailable ? _("Update available.") : _("You are up to date."));

        std::string body;
        body += latestRelease->name.empty() ? latestRelease->tagName : latestRelease->name;
        body += "\n\n";
        body += latestRelease->body.empty() ? _("No release notes were published for this release.") : latestRelease->body;
        if (!latestRelease->assets.empty()) {
            body += "\n\n";
            body += _("Download asset:");
            body += "\n";
            body += latestRelease->assets.front().name;
        }
        setBodyText(body);

        gtk_widget_set_sensitive(refreshButton, true);
        gtk_widget_set_sensitive(downloadButton, updateAvailable && !latestRelease->assets.empty());
    }

    void finishRequest() {
        requestInFlight = false;
        if (destroyed) {
            delete this;
        }
    }

    void startCheck() {
        auto curl = g_find_program_in_path("curl");
        if (!curl) {
            setErrorState(_("Automatic update checks need curl in PATH for now."));
            return;
        }

        setCheckingState();
        requestInFlight = true;

        GError* error = nullptr;
        const auto flags =
                static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);
        auto* process = g_subprocess_new(flags, &error, curl,
                                         "--fail", "--silent", "--show-error", "--location", "--max-time", "15", "-H",
                                         "Accept: application/vnd.github+json", "-H", "User-Agent: VertexNote",
                                         GITHUB_LATEST_RELEASE_API, nullptr);
        g_free(curl);

        if (error) {
            const auto message = std::string{_("Could not start curl: ")} + error->message;
            g_error_free(error);
            setErrorState(message);
            finishRequest();
            return;
        }

        g_subprocess_communicate_utf8_async(
                process, nullptr, nullptr,
                +[](GObject* source, GAsyncResult* result, gpointer data) {
                    auto* self = static_cast<UpdateDialogController*>(data);
                    gchar* stdoutData = nullptr;
                    gchar* stderrData = nullptr;
                    GError* error = nullptr;
                    const auto ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(source), result, &stdoutData,
                                                                         &stderrData, &error);

                    if (!self->destroyed) {
                        if (!ok || error) {
                            std::string message = _("GitHub release check failed.");
                            if (error) {
                                message += "\n";
                                message += error->message;
                            } else if (stderrData) {
                                message += "\n";
                                message += stderrData;
                            }
                            self->setErrorState(message);
                        } else if (auto release = vn::update::parseGithubRelease(stdoutData ? stdoutData : "")) {
                            self->setReleaseState(std::move(*release));
                        } else {
                            self->setErrorState(_("GitHub returned release data that VertexNote could not parse."));
                        }
                    }

                    if (error) {
                        g_error_free(error);
                    }
                    g_free(stdoutData);
                    g_free(stderrData);
                    g_object_unref(source);
                    self->finishRequest();
                },
                this);
    }

    GtkDialog* dialog = nullptr;
    GtkLabel* statusLabel = nullptr;
    GtkLabel* latestLabel = nullptr;
    GtkTextView* textView = nullptr;
    GtkWidget* refreshButton = nullptr;
    GtkWidget* releaseButton = nullptr;
    GtkWidget* downloadButton = nullptr;
    std::optional<vn::update::ReleaseInfo> latestRelease;
    bool requestInFlight = false;
    bool destroyed = false;
};

}  // namespace

void UpdateDialog::show(GtkWindow* parent) { new UpdateDialogController(parent); }

}  // namespace xoj::popup
