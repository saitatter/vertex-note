#include "UpdateDialog.h"

#include <gio/gio.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "control/settings/Settings.h"
#include "util/AppMessageBox.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"
#include "vertexnote/update/GithubReleaseParser.h"
#include "vertexnote/update/ReleaseAssetSelector.h"
#include "vertexnote/update/ReleaseInfo.h"
#include "vertexnote/update/VersionComparator.h"

#include "config.h"

namespace vn::popup {
namespace {

constexpr auto RELEASES_URL = "https://github.com/saitatter/vertex-note/releases";
constexpr auto GITHUB_API_HOST = "api.github.com";
constexpr auto GITHUB_LATEST_RELEASE_PATH = "/repos/saitatter/vertex-note/releases/latest";

auto lower(std::string_view value) -> std::string {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

auto decodeChunkedBody(std::string_view body) -> std::optional<std::string> {
    std::string decoded;
    std::size_t offset = 0;

    while (offset < body.size()) {
        const auto sizeEnd = body.find("\r\n", offset);
        if (sizeEnd == std::string_view::npos) {
            return std::nullopt;
        }

        auto sizeLine = body.substr(offset, sizeEnd - offset);
        if (const auto extension = sizeLine.find(';'); extension != std::string_view::npos) {
            sizeLine = sizeLine.substr(0, extension);
        }

        std::size_t chunkSize = 0;
        const auto* begin = sizeLine.data();
        const auto* end = sizeLine.data() + sizeLine.size();
        const auto [ptr, ec] = std::from_chars(begin, end, chunkSize, 16);
        if (ec != std::errc{} || ptr != end) {
            return std::nullopt;
        }

        offset = sizeEnd + 2;
        if (chunkSize == 0) {
            return decoded;
        }
        if (offset + chunkSize + 2 > body.size()) {
            return std::nullopt;
        }

        decoded.append(body.substr(offset, chunkSize));
        offset += chunkSize;
        if (body.substr(offset, 2) != "\r\n") {
            return std::nullopt;
        }
        offset += 2;
    }

    return std::nullopt;
}

auto fetchLatestReleaseJson() -> std::string {
    GError* error = nullptr;
    auto* client = g_socket_client_new();
    g_socket_client_set_tls(client, true);
    g_socket_client_set_timeout(client, 15);

    auto* connection = g_socket_client_connect_to_host(client, GITHUB_API_HOST, 443, nullptr, &error);
    g_object_unref(client);
    if (error) {
        const std::string message = error->message;
        g_error_free(error);
        throw std::runtime_error(message);
    }

    auto* stream = G_IO_STREAM(connection);
    auto* output = g_io_stream_get_output_stream(stream);
    const std::string request = std::string{"GET "} + GITHUB_LATEST_RELEASE_PATH +
                                " HTTP/1.1\r\n"
                                "Host: " +
                                GITHUB_API_HOST +
                                "\r\n"
                                "User-Agent: VertexNote\r\n"
                                "Accept: application/vnd.github+json\r\n"
                                "Connection: close\r\n\r\n";

    gsize bytesWritten = 0;
    if (!g_output_stream_write_all(output, request.data(), request.size(), &bytesWritten, nullptr, &error)) {
        const std::string message = error ? error->message : "Could not write HTTP request.";
        if (error) {
            g_error_free(error);
        }
        g_object_unref(connection);
        throw std::runtime_error(message);
    }

    std::string response;
    auto* input = g_io_stream_get_input_stream(stream);
    std::array<char, 4096> buffer{};
    while (true) {
        const auto read = g_input_stream_read(input, buffer.data(), buffer.size(), nullptr, &error);
        if (read < 0) {
            const std::string message = error ? error->message : "Could not read HTTP response.";
            if (error) {
                g_error_free(error);
            }
            g_object_unref(connection);
            throw std::runtime_error(message);
        }
        if (read == 0) {
            break;
        }
        response.append(buffer.data(), static_cast<std::size_t>(read));
    }
    g_object_unref(connection);

    const auto headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        throw std::runtime_error("GitHub returned an invalid HTTP response.");
    }

    const auto headers = response.substr(0, headerEnd);
    auto body = response.substr(headerEnd + 4);
    if (headers.find(" 200 ") == std::string::npos) {
        throw std::runtime_error("GitHub returned a non-success HTTP response.");
    }

    if (lower(headers).find("transfer-encoding: chunked") != std::string::npos) {
        auto decoded = decodeChunkedBody(body);
        if (!decoded) {
            throw std::runtime_error("GitHub returned a chunked response VertexNote could not decode.");
        }
        body = std::move(*decoded);
    }

    return body;
}

class UpdateDialogController {
public:
    explicit UpdateDialogController(GtkWindow* parent, Settings* settings):
            dialog(GTK_DIALOG(gtk_dialog_new_with_buttons(_("VertexNote Updates"), parent,
                                                          static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL |
                                                                                      GTK_DIALOG_DESTROY_WITH_PARENT),
                                                          _("_Close"), GTK_RESPONSE_CLOSE, nullptr))),
            settings(settings),
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

        auto* autoCheck = gtk_check_button_new_with_label(_("Check automatically on startup"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoCheck),
                                     settings && settings->isVertexNoteAutomaticUpdateCheckEnabled());
        g_signal_connect(autoCheck, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                             auto* self = static_cast<UpdateDialogController*>(data);
                             if (self->settings) {
                                 self->settings->setVertexNoteAutomaticUpdateCheckEnabled(
                                         gtk_toggle_button_get_active(button));
                             }
                         }),
                         this);
        gtk_box_append(GTK_BOX(box), autoCheck);

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
                                 AppMessageBox::openURL(GTK_WINDOW(dialog), self->releaseUrl().c_str());
                             } else if (response == GTK_RESPONSE_ACCEPT) {
                                 AppMessageBox::openURL(GTK_WINDOW(dialog), self->downloadUrl().c_str());
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
        if (!selectedAsset) {
            return fallback;
        }
        return selectedAsset->downloadUrl;
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
        selectedAsset = vn::update::selectBestAsset(*latestRelease, vn::update::currentReleasePlatform());
        gtk_label_set_text(latestLabel, latestRelease->tagName.c_str());

        const auto updateAvailable = vn::update::isUpdateAvailable(PROJECT_VERSION, latestRelease->tagName);
        gtk_label_set_text(statusLabel, updateAvailable ? _("Update available.") : _("You are up to date."));

        std::string body;
        body += latestRelease->name.empty() ? latestRelease->tagName : latestRelease->name;
        body += "\n\n";
        body += latestRelease->body.empty() ? _("No release notes were published for this release.") : latestRelease->body;
        if (selectedAsset) {
            body += "\n\n";
            body += FS(_F("Download asset for {1}:") %
                       std::string(vn::update::platformName(vn::update::currentReleasePlatform())));
            body += "\n";
            body += selectedAsset->name;
        } else if (!latestRelease->assets.empty()) {
            body += "\n\n";
            body += _("No platform-specific build asset was found for this system.");
        }
        setBodyText(body);

        gtk_widget_set_sensitive(refreshButton, true);
        gtk_widget_set_sensitive(downloadButton, updateAvailable && selectedAsset.has_value());
    }

    void finishRequest() {
        requestInFlight = false;
        if (destroyed) {
            delete this;
        }
    }

    void startCheck() {
        setCheckingState();
        requestInFlight = true;

        auto* task = g_task_new(nullptr, nullptr,
                                +[](GObject*, GAsyncResult* result, gpointer data) {
                    auto* self = static_cast<UpdateDialogController*>(data);
                    GError* error = nullptr;
                    auto* response = static_cast<std::string*>(g_task_propagate_pointer(G_TASK(result), &error));

                    if (!self->destroyed) {
                        if (error) {
                            std::string message = _("GitHub release check failed.");
                            message += "\n";
                            message += error->message;
                            self->setErrorState(message);
                        } else if (response && (vn::update::parseGithubRelease(*response))) {
                            auto release = vn::update::parseGithubRelease(*response);
                            self->setReleaseState(std::move(*release));
                        } else {
                            self->setErrorState(_("GitHub returned release data that VertexNote could not parse."));
                        }
                    }

                    if (error) {
                        g_error_free(error);
                    }
                    delete response;
                    self->finishRequest();
                },
                this);
        g_task_run_in_thread(task, +[](GTask* task, gpointer, gpointer, GCancellable*) {
            try {
                g_task_return_pointer(task, new std::string(fetchLatestReleaseJson()), nullptr);
            } catch (const std::exception& e) {
                g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", e.what());
            }
        });
        g_object_unref(task);
    }

    GtkDialog* dialog = nullptr;
    Settings* settings = nullptr;
    GtkLabel* statusLabel = nullptr;
    GtkLabel* latestLabel = nullptr;
    GtkTextView* textView = nullptr;
    GtkWidget* refreshButton = nullptr;
    GtkWidget* releaseButton = nullptr;
    GtkWidget* downloadButton = nullptr;
    std::optional<vn::update::ReleaseInfo> latestRelease;
    std::optional<vn::update::ReleaseAsset> selectedAsset;
    bool requestInFlight = false;
    bool destroyed = false;
};

}  // namespace

void UpdateDialog::show(GtkWindow* parent, Settings* settings) { new UpdateDialogController(parent, settings); }

}  // namespace vn::popup
