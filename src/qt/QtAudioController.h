/*
 * VertexNote
 *
 * Qt audio controller for recording and playback actions.
 */

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>

#include "config-features.h"
#include "filesystem.h"

class AudioPlayer;
class AudioRecorder;
class Element;
class Settings;

namespace portaudio {
class AutoSystem;
}

struct QtSettings;

struct QtAudioPlaybackTarget {
    fs::path path;
    unsigned int timestamp = 0U;
};

class QtAudioController: public QObject {
    Q_OBJECT

public:
    explicit QtAudioController(QObject* parent = nullptr);
    ~QtAudioController() override;

    void applySettings(const QtSettings& settings);

    [[nodiscard]] auto isAudioAvailable() const -> bool;
    [[nodiscard]] auto isRecording() const -> bool;
    [[nodiscard]] auto isPlaying() const -> bool;
    [[nodiscard]] auto isPaused() const -> bool;
    [[nodiscard]] auto hasCurrentPlayback() const -> bool;
    [[nodiscard]] auto hasLastRecording() const -> bool;
    [[nodiscard]] auto canStartPlayback(const std::vector<const Element*>& selectedElements,
                                        const std::optional<fs::path>& sourceDocumentPath) const -> bool;
    [[nodiscard]] auto lastRecordingPath() const -> const fs::path&;

    [[nodiscard]] auto toggleRecording() -> bool;
    [[nodiscard]] auto togglePausePlayback(const std::vector<const Element*>& selectedElements,
                                           const std::optional<fs::path>& sourceDocumentPath) -> bool;
    [[nodiscard]] auto stopPlayback() -> bool;
    [[nodiscard]] auto seekBackwards() -> bool;
    [[nodiscard]] auto seekForwards() -> bool;

    [[nodiscard]] static auto resolvePlaybackTarget(const std::vector<const Element*>& selectedElements,
                                                    const std::optional<fs::path>& sourceDocumentPath,
                                                    const fs::path& audioFolder)
            -> std::optional<QtAudioPlaybackTarget>;

Q_SIGNALS:
    void audioStateChanged();
    void statusMessage(const QString& text, int timeoutMs);

private:
    [[nodiscard]] auto ensureAudioFolder() -> fs::path;
    [[nodiscard]] auto buildRecordingPath() -> fs::path;
    [[nodiscard]] auto choosePlaybackTarget(const std::vector<const Element*>& selectedElements,
                                            const std::optional<fs::path>& sourceDocumentPath) const
            -> std::optional<QtAudioPlaybackTarget>;
    [[nodiscard]] auto startPlayback(const QtAudioPlaybackTarget& target) -> bool;
    void handlePlaybackStateChange(bool playing, bool paused);

private:
    std::unique_ptr<Settings> settings;
#ifdef ENABLE_AUDIO
    std::unique_ptr<portaudio::AutoSystem> portAudioSystem;
    std::unique_ptr<AudioRecorder> recorder;
    std::unique_ptr<AudioPlayer> player;
#endif
    bool paused = false;
    std::optional<QtAudioPlaybackTarget> currentPlayback;
    fs::path lastRecordedFile;
};
