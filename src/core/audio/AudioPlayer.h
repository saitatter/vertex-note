/*
 * VertexNote
 *
 * Class to play audio from a MP3-file
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>
#include <memory>  // for make_unique, unique_ptr
#include <vector>  // for vector

#include "filesystem.h"  // for path

template <typename T>
class AudioQueue;
class DeviceInfo;
class PortAudioConsumer;
class Settings;
class VorbisProducer;


class AudioPlayer final {
public:
    explicit AudioPlayer(Settings& settings, std::function<void(bool playing, bool paused)> stateObserver = {});
    AudioPlayer(AudioPlayer const&) = delete;
    AudioPlayer(AudioPlayer&&) = delete;
    auto operator=(AudioPlayer const&) -> AudioPlayer& = delete;
    auto operator=(AudioPlayer&&) -> AudioPlayer& = delete;
    ~AudioPlayer();

    bool start(fs::path const& file, unsigned int timestamp = 0);
    bool isPlaying();
    void stop();
    bool play();
    void pause();
    void seek(int seconds);

    std::vector<DeviceInfo> getOutputDevices();

    Settings& getSettings();
    void disableAudioPlaybackButtons();

private:
    void notifyPlaybackState(bool playing, bool paused);

    Settings& settings;
    std::function<void(bool playing, bool paused)> stateObserver;

    std::unique_ptr<AudioQueue<float>> audioQueue;
    std::unique_ptr<PortAudioConsumer> portAudioConsumer;
    std::unique_ptr<VorbisProducer> vorbisProducer;
};
