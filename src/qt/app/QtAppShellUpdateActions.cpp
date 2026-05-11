/*
 * VertexNote
 *
 * Qt app shell update workflow.
 */

#include "QtAppShell.h"

#include <exception>
#include <string>
#include <thread>

#include <QMetaObject>
#include <QPointer>

#include "config.h"
#include "vertexnote/update/GithubReleaseParser.h"
#include "vertexnote/update/ReleaseAssetSelector.h"
#include "vertexnote/update/ReleaseFetcher.h"
#include "vertexnote/update/VersionComparator.h"
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
