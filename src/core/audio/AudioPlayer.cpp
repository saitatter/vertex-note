#include "AudioPlayer.h"

#include "audio/AudioQueue.h"                // for AudioQueue
#include "audio/DeviceInfo.h"                // for DeviceInfo
#include "audio/PortAudioConsumer.h"         // for PortAudioConsumer
#include "audio/VorbisProducer.h"            // for VorbisProducer

class Settings;

AudioPlayer::AudioPlayer(Settings& settings, std::function<void(bool playing, bool paused)> stateObserver):
        settings(settings),
        stateObserver(std::move(stateObserver)),
        audioQueue(std::make_unique<AudioQueue<float>>()),
        portAudioConsumer(std::make_unique<PortAudioConsumer>(*this, *audioQueue)),
        vorbisProducer(std::make_unique<VorbisProducer>(*audioQueue)) {}

AudioPlayer::~AudioPlayer() { this->stop(); }

auto AudioPlayer::start(fs::path const& file, unsigned int timestamp) -> bool {
    // Start the producer for reading the data
    bool status = this->vorbisProducer->start(file, timestamp);

    // Start playing
    if (status) {
        status = status && this->play();
    }

    return status;
}

auto AudioPlayer::isPlaying() -> bool { return this->portAudioConsumer->isPlaying(); }

void AudioPlayer::notifyPlaybackState(bool playing, bool paused) {
    if (this->stateObserver) {
        this->stateObserver(playing, paused);
    }
}

void AudioPlayer::pause() {
    if (!this->portAudioConsumer->isPlaying()) {
        return;
    }

    // Stop playing audio
    this->portAudioConsumer->stopPlaying();
    notifyPlaybackState(false, true);
}

auto AudioPlayer::play() -> bool {
    if (this->portAudioConsumer->isPlaying()) {
        return false;
    }

    const bool started = this->portAudioConsumer->startPlaying();
    if (started) {
        notifyPlaybackState(true, false);
    }
    return started;
}

void AudioPlayer::disableAudioPlaybackButtons() {
    if (this->audioQueue->hasStreamEnded()) {
        notifyPlaybackState(false, false);
    }
}

void AudioPlayer::stop() {
    // Stop playing audio
    this->portAudioConsumer->stopPlaying();

    this->audioQueue->signalEndOfStream();
    disableAudioPlaybackButtons();

    // Abort libsox
    this->vorbisProducer->abort();

    // Reset the queue for the next playback
    this->audioQueue->reset();
    notifyPlaybackState(false, false);
}

void AudioPlayer::seek(int seconds) {
    // set seek flag here in vorbisProducer
    this->vorbisProducer->seek(seconds);
}

auto AudioPlayer::getOutputDevices() -> std::vector<DeviceInfo> { return this->portAudioConsumer->getOutputDevices(); }

auto AudioPlayer::getSettings() -> Settings& { return this->settings; }
