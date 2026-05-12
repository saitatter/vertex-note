#include "VorbisConsumer.h"

#include <algorithm>  // for for_each, min, max
#include <cstddef>    // for size_t
#include <deque>      // for _Deque_iterator
#include <iostream>   // for cerr
#include <iterator>   // for back_insert_iterator, back_in...
#include <memory>     // for unique_ptr
#include <string>     // for string
#include <utility>    // for move
#include <vector>     // for vector

#include <sndfile.h>  // for SF_INFO, sf_strerror, sf_writ...

#include "audio/AudioQueue.h"           // for AudioQueue
#include "control/settings/Settings.h"  // for Settings
#include "util/StringUtils.h"

#include "SNDFileCpp.h"  // for make_snd_file

auto VorbisConsumer::start(fs::path const& file) -> bool {
    auto [sampleRate, channels] = this->audioQueue.getAudioAttributes();

    if (sampleRate == -1) {
        std::cerr << "VorbisConsumer: Timing issue - Sample rate requested before known" << std::endl;
        return false;
    }

    SF_INFO sfInfo;
    sfInfo.channels = channels;
    sfInfo.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
    sfInfo.samplerate = static_cast<int>(this->settings.getAudioSampleRate());

    auto sfFile = vn::audio::make_snd_file(file.native(), SFM_WRITE, &sfInfo);
    if (!sfFile) {
        std::cerr << "VorbisConsumer: output file \"" << char_cast(file.u8string().c_str())
                  << "\" could not be opened\ncaused by:" << sf_strerror(sfFile.get()) << std::endl;
        return false;
    }

    this->consumerThread = std::thread([this, sfFile = std::move(sfFile), channels = channels] {
        auto lock{audioQueue.acquire_lock()};
        auto buffer_size{size_t(64 * channels)};
        std::vector<float> buffer;
        buffer.reserve(buffer_size);  // efficiency
        float audioGain = static_cast<float>(this->settings.getAudioGain());

        while (!(this->stopConsumer || (audioQueue.hasStreamEnded() && audioQueue.empty()))) {
            audioQueue.waitForProducer(lock);
            while (audioQueue.size() > buffer_size || (audioQueue.hasStreamEnded() && !audioQueue.empty())) {
                buffer.resize(0);
                this->audioQueue.pop(std::back_inserter(buffer), buffer_size);
                // apply gain
                if (audioGain != 1.0f) {
                    std::for_each(begin(buffer), end(buffer), [audioGain](auto& val) { val *= audioGain; });
                }
                sf_writef_float(sfFile.get(), buffer.data(),
                                std::min<sf_count_t>(sf_count_t(buffer.size()) / channels, 64));
            }
        }
    });
    return true;
}

void VorbisConsumer::join() {
    // Join the consumer thread to wait for completion
    if (this->consumerThread.joinable()) {
        this->consumerThread.join();
    }
}

void VorbisConsumer::stop() {
    // Stop consumer
    this->audioQueue.signalEndOfStream();

    // Wait for consumer to finish
    join();
}
