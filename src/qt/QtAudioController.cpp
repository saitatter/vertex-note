/*
 * VertexNote
 *
 * Qt audio controller for recording and playback actions.
 */

#include "QtAudioController.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <utility>

#include "QtSettingsDialog.h"
#include "audio/AudioPlayer.h"
#include "audio/AudioRecorder.h"
#include "audio/DeviceInfo.h"
#include "control/settings/Settings.h"
#include "model/AudioElement.h"
#include "util/PathUtil.h"

#ifdef ENABLE_AUDIO
#include <portaudiocpp/PortAudioCpp.hxx>
#endif

namespace {

auto defaultAudioFolderPath() -> fs::path { return Util::ensureFolderExists(Util::getDataSubfolder("audio")); }

auto hasUsableRelativeParent(const fs::path& path) -> bool {
    if (!path.has_parent_path()) {
        return false;
    }
    const auto parent = path.parent_path();
    return !parent.empty() && parent != "." && parent != "/";
}

}  // namespace

QtAudioController::QtAudioController(QObject* parent): QObject(parent), settings(std::make_unique<Settings>(fs::path{})) {
#ifdef ENABLE_AUDIO
    this->settings->setAudioFolder(defaultAudioFolderPath());
    this->portAudioSystem = std::make_unique<portaudio::AutoSystem>();
    this->recorder = std::make_unique<AudioRecorder>(*this->settings);
    this->player = std::make_unique<AudioPlayer>(*this->settings, [this](bool playing, bool paused) {
        handlePlaybackStateChange(playing, paused);
    });
#endif
}

QtAudioController::~QtAudioController() = default;

void QtAudioController::applySettings(const QtSettings& qtSettings) {
#ifdef ENABLE_AUDIO
    const bool wasAvailable = isAudioAvailable();
    this->settings->setAudioDisabled(qtSettings.disableAudio);
    this->settings->setAudioInputDevice(qtSettings.audioInputDevice);
    this->settings->setAudioOutputDevice(qtSettings.audioOutputDevice);
    this->settings->setAudioFolder(qtSettings.audioFolder.empty() ? defaultAudioFolderPath()
                                                                  : fs::path(qtSettings.audioFolder));
    this->settings->setAudioSampleRate(qtSettings.audioSampleRate);
    this->settings->setAudioGain(qtSettings.audioGain);
    this->settings->setDefaultSeekTime(static_cast<unsigned int>(std::max(1, qtSettings.defaultSeekTimeSeconds)));
    if (wasAvailable && qtSettings.disableAudio) {
        if (this->recorder && this->recorder->isRecording()) {
            this->recorder->stop();
        }
        if (this->player && this->player->isPlaying()) {
            this->player->stop();
        }
        this->paused = false;
        this->currentPlayback.reset();
        Q_EMIT audioStateChanged();
    }
#else
    (void)qtSettings;
#endif
}

auto QtAudioController::isAudioAvailable() const -> bool {
#ifdef ENABLE_AUDIO
    return this->recorder != nullptr && this->player != nullptr && !this->settings->isAudioDisabled();
#else
    return false;
#endif
}

auto QtAudioController::inputDeviceOptions() const -> std::vector<QtAudioDeviceOption> {
    std::vector<QtAudioDeviceOption> devices;
#ifdef ENABLE_AUDIO
    if (!this->recorder) {
        return devices;
    }
    for (const auto& device: this->recorder->getInputDevices()) {
        devices.push_back({.index = static_cast<int>(device.getIndex()),
                           .displayName = device.getDeviceName(),
                           .selected = device.getSelected()});
    }
#endif
    return devices;
}

auto QtAudioController::outputDeviceOptions() const -> std::vector<QtAudioDeviceOption> {
    std::vector<QtAudioDeviceOption> devices;
#ifdef ENABLE_AUDIO
    if (!this->player) {
        return devices;
    }
    for (const auto& device: this->player->getOutputDevices()) {
        devices.push_back({.index = static_cast<int>(device.getIndex()),
                           .displayName = device.getDeviceName(),
                           .selected = device.getSelected()});
    }
#endif
    return devices;
}

auto QtAudioController::isRecording() const -> bool {
#ifdef ENABLE_AUDIO
    return this->recorder && this->recorder->isRecording();
#else
    return false;
#endif
}

auto QtAudioController::isPlaying() const -> bool {
#ifdef ENABLE_AUDIO
    return this->player && this->player->isPlaying();
#else
    return false;
#endif
}

auto QtAudioController::isPaused() const -> bool { return this->paused; }

auto QtAudioController::hasCurrentPlayback() const -> bool { return this->currentPlayback.has_value(); }

auto QtAudioController::hasLastRecording() const -> bool { return !this->lastRecordedFile.empty(); }

auto QtAudioController::canStartPlayback(const std::vector<const Element*>& selectedElements,
                                         const std::optional<fs::path>& sourceDocumentPath) const -> bool {
#ifdef ENABLE_AUDIO
    return this->currentPlayback.has_value() ||
           resolvePlaybackTarget(selectedElements, sourceDocumentPath, this->settings->getAudioFolder()).has_value() ||
           hasLastRecording();
#else
    (void)selectedElements;
    (void)sourceDocumentPath;
    return false;
#endif
}

auto QtAudioController::lastRecordingPath() const -> const fs::path& { return this->lastRecordedFile; }

auto QtAudioController::toggleRecording() -> bool {
#ifndef ENABLE_AUDIO
    Q_EMIT statusMessage(QStringLiteral("Audio support was disabled at build time"), 4000);
    return false;
#else
    if (!isAudioAvailable()) {
        Q_EMIT statusMessage(QStringLiteral("Audio backend is not available"), 4000);
        return false;
    }

    if (isRecording()) {
        this->recorder->stop();
        Q_EMIT audioStateChanged();
        Q_EMIT statusMessage(QStringLiteral("Audio recording stopped"), 2500);
        return true;
    }

    if (isPlaying() || this->paused) {
        (void)stopPlayback();
    }

    const auto folder = ensureAudioFolder();
    if (folder.empty()) {
        return false;
    }

    const auto recordingPath = buildRecordingPath();
    if (!this->recorder->start(recordingPath)) {
        this->lastRecordedFile.clear();
        Q_EMIT statusMessage(QStringLiteral("Could not start audio recording"), 4000);
        return false;
    }

    this->lastRecordedFile = recordingPath;
    Q_EMIT audioStateChanged();
    Q_EMIT statusMessage(QStringLiteral("Audio recording started"), 2500);
    return true;
#endif
}

auto QtAudioController::togglePausePlayback(const std::vector<const Element*>& selectedElements,
                                            const std::optional<fs::path>& sourceDocumentPath) -> bool {
#ifndef ENABLE_AUDIO
    Q_EMIT statusMessage(QStringLiteral("Audio support was disabled at build time"), 4000);
    return false;
#else
    if (!isAudioAvailable()) {
        Q_EMIT statusMessage(QStringLiteral("Audio backend is not available"), 4000);
        return false;
    }
    if (isRecording()) {
        Q_EMIT statusMessage(QStringLiteral("Stop recording before playback"), 3000);
        return false;
    }

    if (isPlaying()) {
        this->player->pause();
        Q_EMIT statusMessage(QStringLiteral("Audio paused"), 2500);
        return true;
    }

    if (this->paused && this->currentPlayback.has_value()) {
        if (this->player->play()) {
            Q_EMIT statusMessage(QStringLiteral("Audio playback resumed"), 2500);
            return true;
        }
        Q_EMIT statusMessage(QStringLiteral("Could not resume audio playback"), 4000);
        return false;
    }

    const auto target = choosePlaybackTarget(selectedElements, sourceDocumentPath);
    if (!target) {
        Q_EMIT statusMessage(QStringLiteral("No audio clip is available to play"), 3500);
        return false;
    }
    return startPlayback(*target);
#endif
}

auto QtAudioController::stopPlayback() -> bool {
#ifndef ENABLE_AUDIO
    return false;
#else
    if (!isAudioAvailable() || (!this->currentPlayback.has_value() && !isPlaying() && !this->paused)) {
        return false;
    }

    this->player->stop();
    this->paused = false;
    Q_EMIT audioStateChanged();
    Q_EMIT statusMessage(QStringLiteral("Audio playback stopped"), 2500);
    return true;
#endif
}

auto QtAudioController::seekBackwards() -> bool {
#ifndef ENABLE_AUDIO
    return false;
#else
    if (!isAudioAvailable() || !this->currentPlayback.has_value()) {
        return false;
    }
    this->player->seek(-static_cast<int>(this->settings->getDefaultSeekTime()));
    Q_EMIT statusMessage(QStringLiteral("Seeked backwards"), 1500);
    return true;
#endif
}

auto QtAudioController::seekForwards() -> bool {
#ifndef ENABLE_AUDIO
    return false;
#else
    if (!isAudioAvailable() || !this->currentPlayback.has_value()) {
        return false;
    }
    this->player->seek(static_cast<int>(this->settings->getDefaultSeekTime()));
    Q_EMIT statusMessage(QStringLiteral("Seeked forwards"), 1500);
    return true;
#endif
}

auto QtAudioController::resolvePlaybackTarget(const std::vector<const Element*>& selectedElements,
                                              const std::optional<fs::path>& sourceDocumentPath,
                                              const fs::path& audioFolder) -> std::optional<QtAudioPlaybackTarget> {
    if (selectedElements.size() != 1U) {
        return std::nullopt;
    }

    auto* audioElement = dynamic_cast<const AudioElement*>(selectedElements.front());
    if (!audioElement) {
        return std::nullopt;
    }

    auto audioPath = audioElement->getAudioFilename();
    if (audioPath.empty()) {
        return std::nullopt;
    }

    if (!Util::isAbsolute(audioPath)) {
        if (hasUsableRelativeParent(audioPath) && sourceDocumentPath.has_value()) {
            audioPath = sourceDocumentPath->parent_path() / audioPath;
        } else {
            audioPath = audioFolder / audioPath;
        }
    }

    return QtAudioPlaybackTarget{std::move(audioPath), static_cast<unsigned int>(audioElement->getTimestamp())};
}

auto QtAudioController::ensureAudioFolder() -> fs::path {
#ifdef ENABLE_AUDIO
    try {
        const auto folder = this->settings->getAudioFolder().empty() ? defaultAudioFolderPath()
                                                                     : Util::ensureFolderExists(this->settings->getAudioFolder());
        this->settings->setAudioFolder(folder);
        return folder;
    } catch (...) {
        Q_EMIT statusMessage(QStringLiteral("Could not prepare the audio folder"), 4000);
        return {};
    }
#else
    return {};
#endif
}

auto QtAudioController::buildRecordingPath() -> fs::path {
    const auto folder = ensureAudioFolder();
    if (folder.empty()) {
        return {};
    }

    std::array<char, 64> buffer{};
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif
    std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02d_%02d-%02d-%02d.ogg", localTime.tm_year + 1900,
                  localTime.tm_mon + 1, localTime.tm_mday, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    return folder / buffer.data();
}

auto QtAudioController::choosePlaybackTarget(const std::vector<const Element*>& selectedElements,
                                             const std::optional<fs::path>& sourceDocumentPath) const
        -> std::optional<QtAudioPlaybackTarget> {
    if (this->currentPlayback.has_value()) {
        return this->currentPlayback;
    }

    const fs::path audioFolder =
#ifdef ENABLE_AUDIO
            this->settings->getAudioFolder();
#else
            fs::path{};
#endif
    if (auto selectionTarget = resolvePlaybackTarget(selectedElements, sourceDocumentPath, audioFolder)) {
        return selectionTarget;
    }

    if (!this->lastRecordedFile.empty()) {
        return QtAudioPlaybackTarget{this->lastRecordedFile, 0U};
    }

    return std::nullopt;
}

auto QtAudioController::startPlayback(const QtAudioPlaybackTarget& target) -> bool {
#ifndef ENABLE_AUDIO
    return false;
#else
    if (!fs::exists(target.path)) {
        Q_EMIT statusMessage(QStringLiteral("The selected audio file could not be found"), 4000);
        return false;
    }

    this->player->stop();
    this->currentPlayback = target;
    this->paused = false;
    if (!this->player->start(target.path, target.timestamp)) {
        this->currentPlayback.reset();
        Q_EMIT audioStateChanged();
        Q_EMIT statusMessage(QStringLiteral("Could not start audio playback"), 4000);
        return false;
    }

    Q_EMIT audioStateChanged();
    Q_EMIT statusMessage(QStringLiteral("Audio playback started"), 2500);
    return true;
#endif
}

void QtAudioController::handlePlaybackStateChange(bool playing, bool paused) {
    this->paused = paused;
    if (!playing && !paused && !this->currentPlayback.has_value() && !this->lastRecordedFile.empty()) {
        this->currentPlayback = QtAudioPlaybackTarget{this->lastRecordedFile, 0U};
    }
    Q_EMIT audioStateChanged();
}
