#pragma once

#include "../VisualizationPlugin.h"

// Stereo VU meter with RMS levels, colour zones, and peak-hold ticks.
class VUMeterPlugin : public VisualizationPlugin
{
public:
    VUMeterPlugin();

    juce::String getName() const override { return "VU Meter"; }

    void update(const juce::AudioBuffer<float>& buffer,
                const juce::Array<float>& spectrum) override;

    void render(juce::Graphics& g, juce::Rectangle<int> bounds) override;

private:
    static constexpr int kPeakHoldTicks = 50; // ~2 s at 25 fps
    static constexpr float kDecayRate   = 0.04f;

    // Smoothed RMS level in dBFS per channel
    float levelL = -60.0f, levelR = -60.0f;
    // Peak hold level in dBFS per channel
    float peakL  = -60.0f, peakR  = -60.0f;
    int   peakCounterL = 0, peakCounterR = 0;

    static float rmsToDb(const float* data, int numSamples);
    void drawBar(juce::Graphics& g, juce::Rectangle<int> rect,
                 float levelDb, float peakDb) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUMeterPlugin)
};
