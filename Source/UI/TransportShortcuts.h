#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/AudioEngine.h"
#include "../Playlist/PlaylistManager.h"

// Playback keyboard shortcuts shared by the main window and the fullscreen
// windows (visualizer, pattern viewer): Space toggles play/pause, Left/Right
// skip to the previous/next track, Backspace stops. Returns true if `key`
// was one of these and was acted on.
inline bool handleTransportKeyPress(const juce::KeyPress& key, AudioEngine* audioEngine, PlaylistManager* playlistManager)
{
    if (key == juce::KeyPress::spaceKey)
    {
        if (audioEngine->isPlaying())
            audioEngine->pause();
        else
            audioEngine->play();
        return true;
    }
    if (key == juce::KeyPress::leftKey)
    {
        playlistManager->previousTrack();
        return true;
    }
    if (key == juce::KeyPress::rightKey)
    {
        playlistManager->nextTrack();
        return true;
    }
    if (key == juce::KeyPress::backspaceKey)
    {
        audioEngine->stop();
        return true;
    }
    return false;
}
