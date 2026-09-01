#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/AudioEngine.h"
#include "../Playlist/PlaylistManager.h"

// Generic kiosk-mode top-level window that temporarily reparents an existing
// component full screen, and puts it back where it came from (same parent,
// same bounds) when the user presses Escape. Unlike FullScreenVisualizerWindow
// (which owns a fresh child), this hosts a component that is fed by, and
// keeps its state in, its normal owner -- e.g. the pattern/sequencer viewer,
// whose playback position is pushed in from MainComponent's timer. Also
// answers the shared transport shortcuts (space/left/right/backspace).
class FullScreenHostWindow : public juce::Component,
                             private juce::KeyListener
{
public:
    // `content` must outlive this window (it always does here: MainComponent
    // owns both the content and, indirectly via the onClosed callback, this
    // window's lifetime).
    FullScreenHostWindow(juce::Component& content, AudioEngine* engine, PlaylistManager* playlist,
                         std::function<void()> onClosed);
    ~FullScreenHostWindow() override;

    // Adds this to the desktop, enters kiosk mode, reparents content in, and
    // grabs keyboard focus.
    void show();

    void resized() override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originator) override;

private:
    // Reparents content back to where it came from. Safe to call more than
    // once (e.g. from both keyPressed and the destructor).
    void restoreContent();

    juce::Component& content;
    AudioEngine* audioEngine;
    PlaylistManager* playlistManager;
    juce::Component* originalParent = nullptr;
    juce::Rectangle<int> originalBounds;
    bool restored = false;

    std::function<void()> onClosed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FullScreenHostWindow)
};
