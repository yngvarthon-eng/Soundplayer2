#pragma once

#include "../VisualizationPlugin.h"
#include <vector>

// Oscilloscope-style waveform display with zero-crossing trigger stabilization.
class WaveformPlugin : public VisualizationPlugin
{
public:
    WaveformPlugin() = default;

    juce::String getName() const override { return "Waveform"; }

    void update(const juce::AudioBuffer<float>& buffer,
                const juce::Array<float>& spectrum) override;

    void render(juce::Graphics& g, juce::Rectangle<int> bounds) override;

private:
    std::vector<float> waveformData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformPlugin)
};
