#pragma once

#include "../VisualizationPlugin.h"
#include <vector>

// Log-spaced spectrum bars arranged in a ring, pulsing outward from a centre
// disc whose radius tracks overall signal energy.
class RadialSpectrumPlugin : public VisualizationPlugin
{
public:
    RadialSpectrumPlugin();

    juce::String getName() const override { return "Radial Spectrum"; }

    void update(const juce::AudioBuffer<float>& buffer,
                const juce::Array<float>& spectrum) override;

    void render(juce::Graphics& g, juce::Rectangle<int> bounds) override;

private:
    static constexpr int kNumBars = 96;

    std::vector<float> barHeights; // smoothed display heights (0..1)

    static constexpr float kDecayRate = 0.05f;
    static constexpr float kRiseRate  = 0.35f;

    // Smoothed overall energy, drives the centre disc radius and slow rotation.
    float energy = 0.0f;
    float rotation = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RadialSpectrumPlugin)
};
