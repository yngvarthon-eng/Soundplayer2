#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/AudioEngine.h"
#include "../Visualization/SpectrumAnalyzer.h"
#include "../Visualization/VisualizationPlugin.h"

class VisualizationComponent : public juce::Component,
                               public juce::Timer
{
public:
    explicit VisualizationComponent(AudioEngine* engine);
    ~VisualizationComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;

    // Accessors used by VideoExporter to mirror "whatever tab is currently shown".
    int getActivePluginIndex() const { return activePlugin; }
    int getNumPlugins() const { return plugins.size(); }
    juce::String getPluginName(int index) const;

    // Used by the fullscreen visualizer window to open on the same tab as the
    // embedded view it mirrors.
    void setActivePluginIndex(int index);

    // Digit-key shortcut: '1'-'9' selects that tab directly, as an alternative
    // to clicking it. Returns true if `key` was a digit that selected a tab.
    bool handleDigitHotkey(const juce::KeyPress& key);

    // Fresh instance of the plugin at `index` (same type as the live tab), for use
    // on the export thread -- never the live UI's own plugin instance.
    std::unique_ptr<VisualizationPlugin> createPluginInstance(int index) const;

private:
    AudioEngine* audioEngine;

    SpectrumAnalyzer spectrumAnalyzer;
    juce::OwnedArray<VisualizationPlugin> plugins;
    int activePlugin = 0;

    static constexpr int kTabH = 22; // height of the tab bar

    void drawTabs(juce::Graphics& g) const;
    juce::Rectangle<int> pluginArea() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisualizationComponent)
};
