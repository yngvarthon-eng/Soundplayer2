#pragma once

#include "../VisualizationPlugin.h"
#include <vector>

// FFT-based spectrum bars with 64 log-spaced bins and peak hold.
class SpectrumBarsPlugin : public VisualizationPlugin
{
public:
    SpectrumBarsPlugin();

    juce::String getName() const override { return "Spectrum"; }

    void update(const juce::AudioBuffer<float>& buffer,
                const juce::Array<float>& spectrum) override;

    void render(juce::Graphics& g, juce::Rectangle<int> bounds) override;

private:
    static constexpr int kNumBars = 64;

    // Smooth and peak-hold state
    std::vector<float> barHeights;  // smoothed display heights (0..1)
    std::vector<float> peakHeights; // peak hold heights (0..1)
    std::vector<int>   peakHoldCounters; // ticks remaining at peak

    static constexpr int kPeakHoldTicks = 38; // ~1.5 s at 25 fps
    static constexpr float kDecayRate   = 0.06f;
    static constexpr float kRiseRate    = 0.3f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumBarsPlugin)
};
