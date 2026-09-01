#include "FullScreenHostWindow.h"
#include "TransportShortcuts.h"

FullScreenHostWindow::FullScreenHostWindow(juce::Component& contentToHost, AudioEngine* engine, PlaylistManager* playlist,
                                           std::function<void()> closedCallback)
    : content(contentToHost), audioEngine(engine), playlistManager(playlist), onClosed(std::move(closedCallback))
{
    originalParent = content.getParentComponent();
    originalBounds = content.getBounds();

    setWantsKeyboardFocus(true);
    addKeyListener(this);
}

FullScreenHostWindow::~FullScreenHostWindow()
{
    restoreContent();
    removeKeyListener(this);

    if (juce::Desktop::getInstance().getKioskModeComponent() == this)
        juce::Desktop::getInstance().setKioskModeComponent(nullptr);

    removeFromDesktop();
}

void FullScreenHostWindow::show()
{
    addToDesktop(juce::ComponentPeer::windowIsTemporary);
    setBounds(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea);

    addAndMakeVisible(content); // reparents content here; addChildComponent auto-removes it from its old parent
    resized();

    setVisible(true);
    toFront(true);

    juce::Desktop::getInstance().setKioskModeComponent(this, false);

    grabKeyboardFocus();
}

void FullScreenHostWindow::resized()
{
    content.setBounds(getLocalBounds());
}

void FullScreenHostWindow::restoreContent()
{
    if (restored)
        return;
    restored = true;

    if (originalParent != nullptr)
    {
        originalParent->addAndMakeVisible(content);
        content.setBounds(originalBounds);
    }
}

bool FullScreenHostWindow::keyPressed(const juce::KeyPress& key, juce::Component* /*originator*/)
{
    if (key == juce::KeyPress::escapeKey)
    {
        restoreContent();

        // Defer: we must not delete this window from inside its own key event dispatch.
        if (onClosed)
        {
            auto callback = onClosed;
            juce::MessageManager::callAsync([callback] { callback(); });
        }
        return true;
    }

    return handleTransportKeyPress(key, audioEngine, playlistManager);
}
