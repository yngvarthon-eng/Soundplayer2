#include "FullScreenVisualizerWindow.h"
#include "TransportShortcuts.h"

FullScreenVisualizerWindow::FullScreenVisualizerWindow(AudioEngine* engine, PlaylistManager* playlist, int initialPluginIndex,
                                                        std::function<void()> closedCallback)
    : audioEngine(engine), playlistManager(playlist), onClosed(std::move(closedCallback))
{
    visualization = std::make_unique<VisualizationComponent>(engine);
    visualization->setActivePluginIndex(initialPluginIndex);
    addAndMakeVisible(*visualization);

    setWantsKeyboardFocus(true);
    addKeyListener(this);
}

FullScreenVisualizerWindow::~FullScreenVisualizerWindow()
{
    removeKeyListener(this);

    if (juce::Desktop::getInstance().getKioskModeComponent() == this)
        juce::Desktop::getInstance().setKioskModeComponent(nullptr);

    removeFromDesktop();
}

void FullScreenVisualizerWindow::show()
{
    // No native decorations/taskbar entry; kiosk mode below takes it fullscreen.
    addToDesktop(juce::ComponentPeer::windowIsTemporary);
    setBounds(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea);
    setVisible(true);
    toFront(true);

    juce::Desktop::getInstance().setKioskModeComponent(this, false);

    grabKeyboardFocus();
}

void FullScreenVisualizerWindow::resized()
{
    visualization->setBounds(getLocalBounds());
}

bool FullScreenVisualizerWindow::keyPressed(const juce::KeyPress& key, juce::Component* /*originator*/)
{
    if (key == juce::KeyPress::escapeKey)
    {
        // Defer: we must not delete this window from inside its own key event dispatch.
        if (onClosed)
        {
            auto callback = onClosed;
            juce::MessageManager::callAsync([callback] { callback(); });
        }
        return true;
    }

    if (visualization->handleDigitHotkey(key))
        return true;

    return handleTransportKeyPress(key, audioEngine, playlistManager);
}
