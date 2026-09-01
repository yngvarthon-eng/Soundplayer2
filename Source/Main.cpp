#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"

#ifndef JUCE_APPLICATION_NAME_STRING
#define JUCE_APPLICATION_NAME_STRING "SoundPlayer2"
#endif

#ifndef JUCE_APPLICATION_VERSION_STRING
#define JUCE_APPLICATION_VERSION_STRING "0.1.0"
#endif

class SoundPlayer2Application : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }

    // On Linux JUCE's built-in single-instance support is a no-op: it forwards the
    // command line via MessageManager::broadcastMessage(), which is unimplemented on
    // Linux (juce_Messaging_linux.cpp). That silently dropped files opened from the
    // file manager while we were already running — including every file but the first
    // when several are opened as separate launches. We therefore allow JUCE to start
    // every instance on Linux and do our own single-instance routing over a localhost
    // socket (see below). On macOS/Windows JUCE's native mechanism works, so keep it.
    bool moreThanOneInstanceAllowed() override
    {
       #if JUCE_LINUX
        return true;
       #else
        return false;
       #endif
    }

    void initialise (const juce::String& commandLine) override
    {
       #if JUCE_LINUX
        // If another instance is already running, hand our files to it and exit.
        if (sendToExistingInstance (commandLine))
        {
            quit();
            return;
        }
       #endif

        mainWindow.reset (new MainWindow (getApplicationName()));

       #if JUCE_LINUX
        startSingleInstanceServer();
       #endif

        openFilesFromCommandLine (commandLine);
    }

    void shutdown() override
    {
       #if JUCE_LINUX
        instanceServer.reset();
       #endif
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    // Called by JUCE on macOS/Windows when a second launch is attempted (e.g. opening
    // a file from the file manager while we are already running). On Linux this path is
    // unused — see sendToExistingInstance()/IncomingConnection instead.
    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        handleIncomingCommandLine (commandLine);
    }

    // Add the files named on a command line to the playlist and bring us to the front.
    void handleIncomingCommandLine (const juce::String& commandLine)
    {
        openFilesFromCommandLine (commandLine);

        if (mainWindow != nullptr)
        {
            mainWindow->setMinimised (false);
            mainWindow->toFront (true);
        }
    }

    // Parse a command line into file paths and hand them to the main window.
    // Handles quoted paths and resolves relative paths against the working dir.
    void openFilesFromCommandLine (const juce::String& commandLine)
    {
        if (commandLine.isEmpty() || mainWindow == nullptr)
            return;

        auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent());
        if (content == nullptr)
            return;

        juce::ArgumentList args (getApplicationName(), commandLine);

        juce::StringArray paths;
        for (const auto& arg : args.arguments)
            paths.add (arg.resolveAsFile().getFullPathName());

        if (! paths.isEmpty())
            content->openFiles (paths);
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour (juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons,
                            true)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);

            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
   #if JUCE_LINUX
    //==============================================================================
    // Linux single-instance routing over a localhost socket. The first instance to
    // launch becomes the server; later launches connect, forward their command line,
    // and quit, so their files are routed into the already-running window.

    // A deterministic port in the IANA dynamic/private range, derived from the app id
    // so different apps don't collide while every instance of this app agrees on it.
    static int getSingleInstancePort()
    {
        const auto hash = juce::String (JUCE_APPLICATION_NAME_STRING "-single-instance").hashCode();
        return 49152 + (int) ((juce::uint32) hash % 16384u);
    }

    static const juce::String& getLocalHost()
    {
        static const juce::String host ("127.0.0.1");
        return host;
    }

    // Receives a forwarded command line from a newly launched instance. Callbacks are
    // delivered on the message thread, so it is safe to touch the UI directly. The
    // object owns its own lifetime and deletes itself once the peer disconnects.
    struct IncomingConnection final : public juce::InterprocessConnection
    {
        explicit IncomingConnection (SoundPlayer2Application& ownerApp) : app (ownerApp) {}

        void connectionMade() override {}

        void connectionLost() override
        {
            // Defer deletion so we never delete ourselves from inside our own callback.
            juce::MessageManager::callAsync ([this] { delete this; });
        }

        void messageReceived (const juce::MemoryBlock& message) override
        {
            app.handleIncomingCommandLine (message.toString());
        }

        SoundPlayer2Application& app;
    };

    struct SingleInstanceServer final : public juce::InterprocessConnectionServer
    {
        explicit SingleInstanceServer (SoundPlayer2Application& ownerApp) : app (ownerApp) {}

        juce::InterprocessConnection* createConnectionObject() override
        {
            return new IncomingConnection (app);
        }

        SoundPlayer2Application& app;
    };

    void startSingleInstanceServer()
    {
        instanceServer = std::make_unique<SingleInstanceServer> (*this);

        // If binding fails (e.g. a stale server still holds the port), carry on without
        // single-instance routing rather than refusing to start.
        if (! instanceServer->beginWaitingForSocket (getSingleInstancePort(), getLocalHost()))
            instanceServer.reset();
    }

    // Try to forward our command line to an already-running instance. Returns true if a
    // running instance accepted the message (in which case this process should quit).
    bool sendToExistingInstance (const juce::String& commandLine)
    {
        if (commandLine.isEmpty())
            return false; // nothing to forward — just become (or stay) a normal instance

        struct ClientConnection final : public juce::InterprocessConnection
        {
            void connectionMade() override {}
            void connectionLost() override {}
            void messageReceived (const juce::MemoryBlock&) override {}
        };

        ClientConnection client;
        if (! client.connectToSocket (getLocalHost(), getSingleInstancePort(), 500))
            return false; // no one listening — we are the first instance

        juce::MemoryBlock payload (commandLine.toRawUTF8(),
                                   (size_t) commandLine.getNumBytesAsUTF8());
        const bool sent = client.sendMessage (payload);
        client.disconnect();
        return sent;
    }

    std::unique_ptr<SingleInstanceServer> instanceServer;
   #endif // JUCE_LINUX

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (SoundPlayer2Application)
