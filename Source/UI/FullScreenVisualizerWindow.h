#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/AudioEngine.h"
#include "../Playlist/PlaylistManager.h"
#include "VisualizationComponent.h"

// A borderless, kiosk-mode top-level window that shows the visualizer full
// screen. Owns its own VisualizationComponent (polling the same AudioEngine
// as the embedded one) so the main window's view is unaffected while this is
// open. Escape closes it; the tab bar still works to switch visualizations,
// as do the shared transport shortcuts (space/left/right/backspace).
class FullScreenVisualizerWindow : public juce::Component,
                                   private juce::KeyListener
{
public:
    // onClosed is invoked (asynchronously, after the key event that triggered
    // the close has finished dispatching) when the user asks to exit; the
    // caller is expected to drop its unique_ptr to this window in response.
    FullScreenVisualizerWindow(AudioEngine* engine, PlaylistManager* playlist, int initialPluginIndex,
                               std::function<void()> onClosed);
    ~FullScreenVisualizerWindow() override;

    // Adds this to the desktop, enters kiosk mode, and grabs keyboard focus.
    void show();

    void resized() override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originator) override;

private:
    AudioEngine* audioEngine;
    PlaylistManager* playlistManager;
    std::unique_ptr<VisualizationComponent> visualization;
    std::function<void()> onClosed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FullScreenVisualizerWindow)
};
