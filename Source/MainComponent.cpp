#include "MainComponent.h"
#include "UI/TransportShortcuts.h"
#include <cmath>

namespace
{
    // Audio and module formats SoundPlayer2 can open (mirrors the Add Files dialog filter).
    bool isSupportedAudioFile(const juce::File& file)
    {
        static const juce::StringArray extensions {
            ".mp3", ".wav", ".flac", ".ogg", ".xtp", ".xtd",
            ".mod", ".xm", ".it", ".s3m", ".mptm", ".umx",
            ".ahx", ".ams", ".amf", ".med", ".mo3", ".mtm",
            ".okt", ".oct", ".stm", ".669", ".umf", ".drm",
            ".digi", ".far", ".mdl", ".mt2", ".dcm", ".dlm",
            ".ptm", ".pcm", ".dbm", ".dsm", ".cdm", ".hvl",
            ".c67", ".sfx", ".imf", ".u2b", ".psm", ".stp", ".symmod"
        };
        return extensions.contains(file.getFileExtension().toLowerCase());
    }

    // Expands a dropped path list into supported audio files, recursing into folders.
    juce::Array<juce::File> collectSupportedFiles(const juce::StringArray& paths)
    {
        juce::Array<juce::File> result;
        for (const auto& path : paths)
        {
            juce::File f(path);
            if (f.isDirectory())
            {
                for (const auto& child : f.findChildFiles(juce::File::findFiles, true))
                    if (isSupportedAudioFile(child))
                        result.add(child);
            }
            else if (f.existsAsFile() && isSupportedAudioFile(f))
            {
                result.add(f);
            }
        }
        return result;
    }
}

MainComponent::MainComponent()
{
    playlistManager   = std::make_unique<PlaylistManager>();
    audioEngine       = std::make_unique<AudioEngine>(playlistManager.get());
    pluginHostManager = std::make_unique<PluginHostManager>();
    pluginHostManager->initialise();

    playerControls = std::make_unique<PlayerControlsComponent>(audioEngine.get(), playlistManager.get());
    playlistEditor = std::make_unique<PlaylistEditorComponent>(playlistManager.get());
    visualization  = std::make_unique<VisualizationComponent>(audioEngine.get());
    patternViewer  = std::make_unique<ModulePatternViewer>();
    equalizerUI    = std::make_unique<EqualizerComponent>(audioEngine.get());
    pluginChainUI  = std::make_unique<PluginChainComponent>(
                         pluginHostManager.get(), &audioEngine->getPluginChain());
    analysisUI     = std::make_unique<AudioAnalysisComponent>();

    collapsePlaylistBtn = std::make_unique<juce::TextButton>("Playlist");
    collapsePlaylistBtn->setClickingTogglesState(true);
    collapsePlaylistBtn->onClick = [this]
    {
        playlistCollapsed = collapsePlaylistBtn->getToggleState();
        playlistEditor->setVisible(!playlistCollapsed);
        resized();
    };

    collapseEqBtn = std::make_unique<juce::TextButton>("Equalizer");
    collapseEqBtn->setClickingTogglesState(true);
    collapseEqBtn->onClick = [this]
    {
        eqCollapsed = collapseEqBtn->getToggleState();
        equalizerUI->setVisible(!eqCollapsed);
        resized();
    };

    collapseVizBtn = std::make_unique<juce::TextButton>("Visualizer");
    collapseVizBtn->setClickingTogglesState(true);
    collapseVizBtn->onClick = [this]
    {
        vizCollapsed = collapseVizBtn->getToggleState();
        visualization->setVisible(!vizCollapsed);
        resized();
    };

    fullscreenVizBtn = std::make_unique<juce::TextButton>("Fullscreen");
    fullscreenVizBtn->onClick = [this] { openFullscreenVisualizer(); };

    fullscreenPatternBtn = std::make_unique<juce::TextButton>("Sequencer FS");
    fullscreenPatternBtn->onClick = [this] { openFullscreenPatternViewer(); };
    fullscreenPatternBtn->setEnabled(false); // only meaningful once a module is loaded

    collapseFxBtn = std::make_unique<juce::TextButton>("FX Chain");
    collapseFxBtn->setClickingTogglesState(true);
    collapseFxBtn->onClick = [this]
    {
        fxCollapsed = collapseFxBtn->getToggleState();
        pluginChainUI->setVisible(!fxCollapsed);
        resized();
    };

    collapseAnalysisBtn = std::make_unique<juce::TextButton>("Analysis");
    collapseAnalysisBtn->setClickingTogglesState(true);
    collapseAnalysisBtn->onClick = [this]
    {
        analysisCollapsed = collapseAnalysisBtn->getToggleState();
        analysisUI->setVisible(!analysisCollapsed);
        resized();
    };

    exportVideoBtn = std::make_unique<juce::TextButton>("Export Video...");
    exportVideoBtn->onClick = [this] { showExportVideoDialog(); };

    sampleRateBox = std::make_unique<juce::ComboBox>();
    sampleRateBox->onChange = [this]
    {
        int id = sampleRateBox->getSelectedId();
        if (juce::isPositiveAndBelow(id - 1, availableSampleRates.size()))
        {
            // setSampleRate validates against the hardware and reverts on failure;
            // either way, re-sync the selector to the rate the device actually uses.
            audioEngine->setSampleRate(availableSampleRates[id - 1]);
            updateSampleRateSelection();
        }
    };

    bitDepthBox = std::make_unique<juce::ComboBox>();
    bitDepthBox->addItem("16-bit", 1);
    bitDepthBox->addItem("24-bit", 2);
    bitDepthBox->addItem("32-bit", 3);
    bitDepthBox->setSelectedId(1);
    bitDepthBox->onChange = [this]
    {
        int bd = bitDepthBox->getSelectedId() == 1 ? 16 :
                (bitDepthBox->getSelectedId() == 2 ? 24 : 32);
        audioEngine->setBitDepth(bd);
    };

    addAndMakeVisible(*playerControls);
    addAndMakeVisible(*visualization);
    addAndMakeVisible(*patternViewer);
    addAndMakeVisible(*playlistEditor);
    addAndMakeVisible(*equalizerUI);
    addAndMakeVisible(*pluginChainUI);
    addAndMakeVisible(*analysisUI);
    addAndMakeVisible(*collapsePlaylistBtn);
    addAndMakeVisible(*collapseEqBtn);
    addAndMakeVisible(*collapseVizBtn);
    addAndMakeVisible(*fullscreenVizBtn);
    addAndMakeVisible(*fullscreenPatternBtn);
    addAndMakeVisible(*collapseFxBtn);
    addAndMakeVisible(*collapseAnalysisBtn);
    addAndMakeVisible(*exportVideoBtn);
    addAndMakeVisible(*sampleRateBox);
    addAndMakeVisible(*bitDepthBox);

    patternViewer->setVisible(false);

    playlistManager->addChangeListener(this);

    setSize(1200, 800);

    setWantsKeyboardFocus(true);
    addKeyListener(this);

    audioEngine->initialise();
    populateSampleRates(); // after the device is open so we know its real rates
    startTimer(25); // 40 Hz — drives pattern viewer playback cursor

    grabKeyboardFocus(); // so transport shortcuts work before the user clicks anything
}

MainComponent::~MainComponent()
{
    stopTimer();
    removeKeyListener(this);
    if (exportProgressWindow != nullptr)
        exportProgressWindow->exitModalState(0);
    videoExporter.reset();
    playlistManager->removeChangeListener(this);
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* /*originator*/)
{
    if (visualization->handleDigitHotkey(key))
        return true;

    return handleTransportKeyPress(key, audioEngine.get(), playlistManager.get());
}

void MainComponent::timerCallback()
{
    if (patternViewer->isVisible())
    {
        int patIdx = audioEngine->getCurrentPatternIndex();
        int row    = audioEngine->getCurrentPatternRow();
        patternViewer->setPlaybackPosition(patIdx, row);
    }

    if (videoExporter != nullptr)
    {
        exportProgressValue = videoExporter->getProgress();

        if (videoExporter->hasFinished())
        {
            videoExporter->waitForThreadToExit(2000);
            auto result  = videoExporter->getResult();
            auto message = videoExporter->getResultMessage();
            videoExporter.reset();
            closeExportProgressWindow();

            if (result != VideoExporter::Result::Cancelled)
                juce::AlertWindow::showMessageBoxAsync(
                    result == VideoExporter::Result::Success ? juce::AlertWindow::InfoIcon
                                                              : juce::AlertWindow::WarningIcon,
                    "Export Video", message);
        }
    }
}

void MainComponent::populateSampleRates()
{
    auto formatRate = [](double r) -> juce::String
    {
        // Whole-kHz rates as integers (48 kHz); fractional ones to 1 dp (44.1 kHz).
        if (std::fmod(r, 1000.0) == 0.0)
            return juce::String((int)(r / 1000.0)) + " kHz";
        return juce::String(r / 1000.0, 1) + " kHz";
    };

    availableSampleRates = audioEngine->getSupportedSampleRates();

    sampleRateBox->clear(juce::dontSendNotification);
    for (int i = 0; i < availableSampleRates.size(); ++i)
        sampleRateBox->addItem(formatRate(availableSampleRates[i]), i + 1); // ids are 1-based

    // No device / no advertised rates: nothing useful to choose.
    sampleRateBox->setEnabled(availableSampleRates.size() > 0);

    updateSampleRateSelection();
}

void MainComponent::updateSampleRateSelection()
{
    const double current = audioEngine->getCurrentSampleRate();
    for (int i = 0; i < availableSampleRates.size(); ++i)
    {
        if (std::abs(availableSampleRates[i] - current) < 1.0)
        {
            sampleRateBox->setSelectedId(i + 1, juce::dontSendNotification);
            return;
        }
    }
}

void MainComponent::openFiles(const juce::StringArray& paths)
{
    const int firstNewIndex = playlistManager->getItems().size();
    int added = 0;

    for (const auto& path : paths)
    {
        juce::File file(path);
        if (file.existsAsFile())
        {
            playlistManager->addFile(file);
            ++added;
        }
    }

    // Start playing the first newly added track (mirrors double-click-to-play).
    if (added > 0)
        playlistManager->selectItem(firstNewIndex);
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        juce::File f(path);
        if (f.isDirectory() || isSupportedAudioFile(f))
            return true;
    }
    return false;
}

void MainComponent::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    auto toAdd = collectSupportedFiles(files);
    if (toAdd.isEmpty())
        return;

    const int firstNewIndex = playlistManager->getItems().size();

    // Queue while something is already playing; otherwise start the first dropped track.
    const bool wasPlaying = audioEngine->isPlaying();

    playlistManager->addFiles(toAdd);

    if (!wasPlaying)
        playlistManager->selectItem(firstNewIndex);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == playlistManager.get())
    {
        patternViewer->setPatternData(nullptr);
        patternViewer->setVisible(false);
        triggerAsyncUpdate();
    }
}

void MainComponent::handleAsyncUpdate()
{
    bool showViewer = audioEngine->lastLoadedWasModule();
    const auto& moduleData = audioEngine->getModulePatternData();

    patternViewer->setPatternData(showViewer ? &moduleData : nullptr);
    patternViewer->setVisible(showViewer);
    fullscreenPatternBtn->setEnabled(showViewer);

    // If the loaded track stopped being a module while the pattern view was
    // fullscreen (e.g. the playlist advanced to an audio file), close it
    // rather than leaving a blank fullscreen window up.
    if (!showViewer && fullscreenPattern != nullptr)
        fullscreenPattern.reset();

    // Trigger audio analysis for file tracks; streams are not analyzable
    auto* item = playlistManager->getCurrentItem();
    if (item != nullptr && !item->isStream && item->file.existsAsFile())
        analysisUI->analyzeFile(item->file);
    else
        analysisUI->clear();

    resized();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}

void MainComponent::resized()
{
    if (!playerControls || !visualization || !playlistEditor || !equalizerUI
        || !patternViewer || !pluginChainUI || !analysisUI)
        return;

    const int controlsH  = 130;
    const int toolbarH   = 28;
    const int analysisH  = analysisCollapsed ? 0 : 52;
    const int fxH        = fxCollapsed       ? 0 : 120;

    // Bottom row height (playlist + EQ)
    int bottomH = 0;
    if      (!playlistCollapsed && !eqCollapsed)  bottomH = 340;
    else if ( playlistCollapsed && !eqCollapsed)  bottomH = 240;
    else if (!playlistCollapsed &&  eqCollapsed)  bottomH = 280;

    // Remaining space for visualization + pattern viewer
    int remaining = juce::jmax(
        getHeight() - controlsH - toolbarH - analysisH - fxH - bottomH, 100);
    int vizH, patternH;
    if (patternViewer->isVisible())
    {
        vizH     = vizCollapsed ? 0 : 100;
        patternH = juce::jmax(100, remaining - vizH);
    }
    else
    {
        vizH     = vizCollapsed ? 0 : remaining;
        patternH = 0;
    }

    auto area = getLocalBounds();

    // Transport controls
    playerControls->setBounds(area.removeFromTop(controlsH));

    // Toolbar strip
    auto toolbar = area.removeFromTop(toolbarH);
    toolbar.reduce(4, 2);
    collapsePlaylistBtn->setBounds(toolbar.removeFromLeft(90));
    toolbar.removeFromLeft(4);
    collapseEqBtn->setBounds(toolbar.removeFromLeft(90));
    toolbar.removeFromLeft(4);
    collapseVizBtn->setBounds(toolbar.removeFromLeft(90));
    toolbar.removeFromLeft(4);
    fullscreenVizBtn->setBounds(toolbar.removeFromLeft(90));
    toolbar.removeFromLeft(4);
    fullscreenPatternBtn->setBounds(toolbar.removeFromLeft(100));
    toolbar.removeFromLeft(4);
    collapseFxBtn->setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(4);
    collapseAnalysisBtn->setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(4);
    exportVideoBtn->setBounds(toolbar.removeFromLeft(110));
    // Quality selectors right-aligned
    bitDepthBox->setBounds(toolbar.removeFromRight(80));
    toolbar.removeFromRight(5);
    sampleRateBox->setBounds(toolbar.removeFromRight(90));

    // Analysis panel
    if (analysisH > 0)
        analysisUI->setBounds(area.removeFromTop(analysisH));

    // Pattern viewer. While it's reparented into the fullscreen host window
    // (see openFullscreenPatternViewer), still reserve its usual slot here so
    // the panels below don't shift, but don't stomp on its fullscreen bounds.
    if (patternH > 0)
    {
        auto patternArea = area.removeFromTop(patternH);
        if (patternViewer->getParentComponent() == this)
            patternViewer->setBounds(patternArea);
    }

    // Visualization strip
    visualization->setBounds(area.removeFromTop(vizH));

    // FX Chain panel
    if (fxH > 0)
        pluginChainUI->setBounds(area.removeFromTop(fxH));

    // Bottom panels (playlist + EQ)
    if (bottomH > 0)
    {
        if (!playlistCollapsed && !eqCollapsed)
        {
            auto eqArea = area.removeFromRight(340);
            equalizerUI->setBounds(eqArea);
            playlistEditor->setBounds(area);
        }
        else if (!playlistCollapsed)
        {
            playlistEditor->setBounds(area);
        }
        else
        {
            equalizerUI->setBounds(area);
        }
    }
}

void MainComponent::openFullscreenVisualizer()
{
    if (fullscreenViz != nullptr)
        return; // already open

    fullscreenViz = std::make_unique<FullScreenVisualizerWindow>(
        audioEngine.get(), playlistManager.get(), visualization->getActivePluginIndex(),
        [this] { fullscreenViz.reset(); });
    fullscreenViz->show();
}

void MainComponent::openFullscreenPatternViewer()
{
    if (fullscreenPattern != nullptr || !patternViewer->isVisible())
        return; // already open, or no module loaded to show

    fullscreenPattern = std::make_unique<FullScreenHostWindow>(
        *patternViewer, audioEngine.get(), playlistManager.get(),
        [this] { fullscreenPattern.reset(); });
    fullscreenPattern->show();
}

void MainComponent::closeExportProgressWindow()
{
    if (exportProgressWindow != nullptr)
    {
        exportProgressWindow->exitModalState(0);
        exportProgressWindow = nullptr; // self-deletes (deleteWhenDismissed = true)
    }
    exportProgressBar.reset();
}

void MainComponent::showExportVideoDialog()
{
    auto* item = playlistManager->getCurrentItem();
    if (item == nullptr || item->isStream)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Export Video",
            "Load a local audio file or .xtp/.xtd song first.");
        return;
    }

    if (!VideoExporter::isFfmpegAvailable())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Export Video",
            "ffmpeg was not found on PATH. Install ffmpeg to enable video export.");
        return;
    }

    exportFileChooser = std::make_unique<juce::FileChooser>(
        "Export video as...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
            .getChildFile(item->name + ".mp4"),
        "*.mp4");

    const auto sourceFile = item->file;
    const bool isXtp = item->isXtpSequence;
    const bool hasPatternData = item->isXtpSequence || item->isModule;

    exportFileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, sourceFile, isXtp, hasPatternData](const juce::FileChooser& fc)
        {
            auto outputFile = fc.getResult();
            if (outputFile == juce::File{})
                return;

            // Some save-dialog paths on Linux (zenity, in particular) can hand back a
            // path with the suggested ".mp4" extension dropped -- force it back on so
            // the exported file is visibly an .mp4 rather than relying solely on
            // VideoExporter forcing the container format regardless of filename.
            if (!outputFile.hasFileExtension("mp4"))
                outputFile = outputFile.withFileExtension("mp4");

            auto dialog = std::make_unique<VideoExportSettingsDialog>(hasPatternData);
            auto* dialogPtr = dialog.get();
            dialogPtr->onExport = [this, dialogPtr, sourceFile, isXtp, outputFile]
            {
                VideoExporter::Options options = dialogPtr->buildOptions();
                options.sourceFile = sourceFile;
                options.isXtpSequence = isXtp;
                options.outputFile = outputFile;
                startVideoExport(std::move(options));
            };
            VideoExportSettingsDialog::launch(std::move(dialog));
        });
}

void MainComponent::startVideoExport(VideoExporter::Options options)
{
    const int pluginIndex = visualization->getActivePluginIndex();
    auto* vizPtr = visualization.get();
    options.pluginFactory = [vizPtr, pluginIndex] { return vizPtr->createPluginInstance(pluginIndex); };
    options.equalizerGainsDb = audioEngine->getEqualizer().getAllGains();

    const auto sourceFile = options.sourceFile;
    videoExporter = std::make_unique<VideoExporter>(std::move(options));

    exportProgressValue = 0.0;
    exportProgressWindow = new juce::AlertWindow("Exporting Video",
        "Rendering \"" + sourceFile.getFileNameWithoutExtension() + "\"...", juce::AlertWindow::NoIcon);
    exportProgressBar = std::make_unique<juce::ProgressBar>(exportProgressValue);
    exportProgressBar->setSize(300, 24);
    exportProgressWindow->addCustomComponent(exportProgressBar.get());
    exportProgressWindow->addButton("Cancel", 0);
    exportProgressWindow->enterModalState(true,
        juce::ModalCallbackFunction::create([this](int)
        {
            if (videoExporter != nullptr)
                videoExporter->cancel();
            exportProgressWindow = nullptr;
            exportProgressBar.reset();
        }),
        true);

    videoExporter->startExport();
}
