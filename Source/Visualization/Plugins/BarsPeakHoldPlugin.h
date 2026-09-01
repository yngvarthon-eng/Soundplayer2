#pragma once

#include "../VisualizationPlugin.h"
#include <vector>

// Segmented "hardware LED meter" style spectrum bars: each bar is drawn as
// discrete lit blocks (green/yellow/red zones) with a peak cap that free-falls
// under gravity, distinct from the smooth gradient bars in SpectrumBarsPlugin.
class BarsPeakHoldPlugin : public VisualizationPlugin
{
public:
    BarsPeakHoldPlugin();

    juce::String getName() const override { return "Bars + Peak Hold"; }

    void update(const juce::AudioBuffer<float>& buffer,
                const juce::Array<float>& spectrum) override;

    void render(juce::Graphics& g, juce::Rectangle<int> bounds) override;

private:
    static constexpr int kNumBars = 32;
    static constexpr int kNumSegments = 20;

    std::vector<float> barHeights;   // smoothed display heights (0..1)
    std::vector<float> peakPos;      // current peak cap height (0..1)
    std::vector<float> peakVelocity; // fall speed of peak cap (units / frame)
    std::vector<int>   peakHoldCounters;

    static constexpr int kPeakHoldTicks = 20; // frames before the cap starts falling
    static constexpr float kDecayRate   = 0.06f;
    static constexpr float kRiseRate    = 0.35f;
    static constexpr float kGravity     = 0.0025f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarsPeakHoldPlugin)
};
