/*
 * VertexNote
 *
 * Qt app shell audio actions.
 */

#include "QtAppShell.h"
void QtAppShell::toggleAudioRecording() {
    if (this->audioController.toggleRecording()) {
        updateAudioCommandStates();
    }
}

void QtAppShell::toggleAudioPausePlayback() {
    if (this->audioController.togglePausePlayback(selectedElementsForAudioPlayback(),
                                                  this->documentController.sourcePath())) {
        updateAudioCommandStates();
    }
}

void QtAppShell::stopAudioPlayback() {
    if (this->audioController.stopPlayback()) {
        updateAudioCommandStates();
    }
}

void QtAppShell::seekAudioBackwards() {
    if (this->audioController.seekBackwards()) {
        updateAudioCommandStates();
    }
}

void QtAppShell::seekAudioForwards() {
    if (this->audioController.seekForwards()) {
        updateAudioCommandStates();
    }
}
