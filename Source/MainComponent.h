#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioEngine.h"
#include "Playlist/PlaylistManager.h"
#include "UI/PlayerControlsComponent.h"
#include "UI/PlaylistEditorComponent.h"
#include "UI/VisualizationComponent.h"
#include "UI/FullScreenVisualizerWindow.h"
#include "UI/FullScreenHostWindow.h"
#include "UI/ModulePatternViewer.h"
#include "Effects/EqualizerComponent.h"
#include "Plugins/PluginHostManager.h"
#include "UI/PluginChainComponent.h"
#include "UI/AudioAnalysisComponent.h"
#include "Export/VideoExporter.h"
#include "UI/VideoExportSettingsDialog.h"

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::ChangeListener,
                      private juce::AsyncUpdater,
                      private juce::Timer,
                      private juce::KeyListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originator) override;

    // Add the given file paths to the playlist and start playing the first one.
    // Non-existent paths are ignored. Used for command-line / file-association opens.
    void openFiles(const juce::StringArray& paths);

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;

    // FileDragAndDropTarget — drop audio files or folders anywhere in the window
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // Public so it can be invoked via PlayerControlsComponent's record menu
    // ("Record video with Soundplayer2's export function"), not just the
    // (now-removed) standalone toolbar button.
    void showExportVideoDialog();

private:
    // playlistManager must be declared before audioEngine so it is destroyed AFTER audioEngine
    std::unique_ptr<PlaylistManager>            playlistManager;
    std::unique_ptr<AudioEngine>                audioEngine;

    std::unique_ptr<PlayerControlsComponent>    playerControls;
    std::unique_ptr<PlaylistEditorComponent>    playlistEditor;
    std::unique_ptr<VisualizationComponent>     visualization;
    std::unique_ptr<ModulePatternViewer>        patternViewer;
    std::unique_ptr<EqualizerComponent>         equalizerUI;
    std::unique_ptr<PluginHostManager>          pluginHostManager;
    std::unique_ptr<PluginChainComponent>       pluginChainUI;
    std::unique_ptr<AudioAnalysisComponent>     analysisUI;

    // Toolbar: collapse toggles + audio quality selectors
    std::unique_ptr<juce::TextButton>           collapsePlaylistBtn;
    std::unique_ptr<juce::TextButton>           collapseEqBtn;
    std::unique_ptr<juce::TextButton>           collapseVizBtn;
    std::unique_ptr<juce::TextButton>           fullscreenVizBtn;
    std::unique_ptr<juce::TextButton>           fullscreenPatternBtn;
    std::unique_ptr<juce::TextButton>           collapseAnalysisBtn;
    std::unique_ptr<juce::TextButton>           collapseFxBtn;
    std::unique_ptr<juce::ComboBox>             sampleRateBox;
    std::unique_ptr<juce::ComboBox>             bitDepthBox;

    // Video export: exportProgressWindow is a self-deleting AlertWindow
    // (deleteWhenDismissed = true), so it is tracked as a raw, nullable pointer.
    std::unique_ptr<VideoExporter>              videoExporter;

    // Fullscreen visualizer: a separate kiosk-mode top-level window, opened on
    // demand and torn down when the user presses Escape.
    std::unique_ptr<FullScreenVisualizerWindow> fullscreenViz;
    void openFullscreenVisualizer();

    // Fullscreen pattern/sequencer view: reparents the existing patternViewer
    // (rather than owning a second one, since it doesn't self-drive from an
    // engine pointer -- MainComponent's timer pushes its playback position).
    std::unique_ptr<FullScreenHostWindow>       fullscreenPattern;
    void openFullscreenPatternViewer();

    juce::AlertWindow*                          exportProgressWindow = nullptr;
    std::unique_ptr<juce::ProgressBar>          exportProgressBar;
    double                                      exportProgressValue = 0.0;
    std::unique_ptr<juce::FileChooser>          exportFileChooser;

    void startVideoExport(VideoExporter::Options options);
    void closeExportProgressWindow();

    // Fills sampleRateBox from the device's supported rates and selects the current one.
    void populateSampleRates();
    // Selects the entry matching the engine's actual current rate (no callback).
    void updateSampleRateSelection();
    juce::Array<double> availableSampleRates; // indexed by (item id - 1)
    bool playlistCollapsed  = false;
    bool eqCollapsed        = false;
    bool vizCollapsed       = false;
    bool fxCollapsed        = false;
    bool analysisCollapsed  = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
