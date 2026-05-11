#include "config-features.h"

#ifndef ENABLE_LEGACY_GTK_SHELL

#include "util/AppMessageBox.h"

#include <cstdlib>
#include <string_view>

#include <glib.h>

AppMessageBox::AppMessageBox(GtkDialog* dialog, vn::util::move_only_function<void(int)> callback, CallbackPolicy pol):
        window(GTK_WINDOW(dialog)), callback(std::move(callback)), signalId(0), policy(pol) {}

void AppMessageBox::setDefaultWindow(GtkWindow* win) { (void) win; }

void AppMessageBox::askQuestion(GtkWindow* win, const std::string& maintext, const std::string& secondarytext,
                                const std::vector<Button>& buttons,
                                vn::util::move_only_function<void(int)> callback) {
    (void) win;
    (void) secondarytext;
    g_message("%s", maintext.c_str());
    callback(buttons.empty() ? 0 : buttons.front().response);
}

void AppMessageBox::askQuestionWithMarkup(GtkWindow* win, std::string_view maintext, const std::string& secondarytext,
                                          const std::vector<Button>& buttons,
                                          vn::util::move_only_function<void(int)> callback) {
    askQuestion(win, std::string(maintext), secondarytext, buttons, std::move(callback));
}

void AppMessageBox::showMarkupMessageToUser(GtkWindow* win, const std::string_view& markupTitle,
                                            const std::string& msg, GtkMessageType type) {
    showMessageToUser(win, std::string(markupTitle), msg, type);
}

void AppMessageBox::showMessageToUser(GtkWindow* win, const std::string& msg, GtkMessageType type) {
    showMessageToUser(win, {}, msg, type);
}

void AppMessageBox::showMessageToUser(GtkWindow* win, const std::string& title, const std::string& msg,
                                      GtkMessageType type) {
    (void) win;
    const auto* prefix = type == GTK_MESSAGE_ERROR ? "error" : "message";
    if (title.empty()) {
        g_message("VertexNote %s: %s", prefix, msg.c_str());
    } else {
        g_message("VertexNote %s: %s: %s", prefix, title.c_str(), msg.c_str());
    }
}

void AppMessageBox::showErrorToUser(GtkWindow* win, const std::string& msg) {
    showMessageToUser(win, msg, GTK_MESSAGE_ERROR);
}

void AppMessageBox::showErrorAndQuit(std::string& msg, int exitCode) {
    g_critical("%s", msg.c_str());
    std::exit(exitCode);
}

void AppMessageBox::showPluginMessage(const std::string& pluginName, const std::string& msg, bool error) {
    showMessageToUser(nullptr, pluginName, msg, error ? GTK_MESSAGE_ERROR : GTK_MESSAGE_INFO);
}

auto AppMessageBox::askPluginQuestion(const std::string& pluginName, const std::string& msg,
                                      const std::vector<Button>& buttons, bool error) -> int {
    showPluginMessage(pluginName, msg, error);
    return buttons.empty() ? 0 : buttons.front().response;
}

void AppMessageBox::showHelp(GtkWindow* win) {
    (void) win;
}

void AppMessageBox::openURL(GtkWindow* win, const char* url) {
    (void) win;
    g_message("VertexNote URL: %s", url);
}

void AppMessageBox::replaceFileQuestion(GtkWindow* win, fs::path file,
                                        vn::util::move_only_function<void(const fs::path&)> writeToFile) {
    (void) win;
    writeToFile(file);
}

#endif
