#include <gtest/gtest.h>

#include "config-features.h"

#ifdef ENABLE_AUDIO

#include <portaudiocpp/PortAudioCpp.hxx>

#include "audio/AudioPlayer.h"
#include "control/settings/Settings.h"

TEST(VertexNoteAudioPlayerQtBridge, CanConstructWithoutGtkControl) {
    Settings settings(fs::path{});
    portaudio::AutoSystem portAudioSystem;

    bool callbackTriggered = false;
    AudioPlayer player(settings, [&callbackTriggered](bool /*playing*/, bool /*paused*/) { callbackTriggered = true; });

    EXPECT_FALSE(player.isPlaying());
    EXPECT_FALSE(callbackTriggered);
}

#endif
