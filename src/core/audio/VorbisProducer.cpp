#include "VorbisProducer.h"

#include <algorithm>  // for fill_n, max
#include <cstdio>     // for size_t, SEEK_CUR, SEEK_SET
#include <iostream>   // for cerr
#include <iterator>   // for begin, end
#include <memory>     // for unique_ptr
#include <string>     // for string
#include <utility>    // for move
#include <vector>     // for vector

#include <sndfile.h>  // for SF_INFO, sf_seek, sf_count_t, sf_readf...

#include "audio/AudioQueue.h"  // for AudioQueue
#include "audio/SNDFileCpp.h"  // for make_snd_file
#include "util/StringUtils.h"

constexpr auto sample_buffer_size = size_t{16384U};

auto VorbisProducer::start(fs::path const& file, unsigned int timestamp) -> bool {
    SF_INFO sfInfo{};
    auto sfFile = vn::audio::make_snd_file(file, SFM_READ, &sfInfo);
    if (!sfFile) {
        std::cerr << "VorbisProducer: input file \"" << char_cast(file.u8string().c_str())
                  << "\" could not be opened\ncaused by:" << sf_strerror(sfFile.get()) << std::endl;
        return false;
    }

    sf_count_t seekPosition = sfInfo.samplerate / 1000 * sf_count_t(timestamp);

    if (seekPosition < sfInfo.frames) {
        sf_seek(sfFile.get(), seekPosition, SEEK_SET);
    } else {
        std::cerr << "VorbisProducer: Seeking outside of audio file extent" << std::endl;
    }

    this->audioQueue.setAudioAttributes(sfInfo.samplerate, static_cast<unsigned int>(sfInfo.channels));

    this->producerThread = std::thread([this, sfInfo, sfFile = std::move(sfFile)] {
        sf_count_t numFrames{1};
        size_t const bufferSize{size_t(1024U) * size_t(sfInfo.channels)};
        std::vector<float> sampleBuffer(bufferSize);
        auto lock = audioQueue.acquire_lock();

        while (!this->stopProducer && numFrames > 0 && !this->audioQueue.hasStreamEnded()) {
            sampleBuffer.resize(bufferSize);
            numFrames = sf_readf_float(sfFile.get(), sampleBuffer.data(), 1024);
            sampleBuffer.resize(size_t(numFrames * sfInfo.channels));

            while (this->audioQueue.size() >= sample_buffer_size && !this->audioQueue.hasStreamEnded() &&
                   !this->stopProducer) {
                audioQueue.waitForConsumer(lock);
            }

            if (auto tmpSeekSeconds = this->seekSeconds.load(); tmpSeekSeconds != 0) {
                sf_seek(sfFile.get(), tmpSeekSeconds * sfInfo.samplerate, SEEK_CUR);
                this->seekSeconds -= tmpSeekSeconds;
            }

            this->audioQueue.emplace(begin(sampleBuffer), end(sampleBuffer));
        }
        this->audioQueue.signalEndOfStream();
    });
    return true;
}

void VorbisProducer::abort() {
    this->stopProducer = true;
    // Wait for producer to finish
    stop();
    this->stopProducer = false;
}

void VorbisProducer::stop() {
    // Wait for producer to finish
    if (this->producerThread.joinable()) {
        this->producerThread.join();
    }
}


void VorbisProducer::seek(int seconds) { this->seekSeconds = seconds; }
