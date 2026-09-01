#pragma once

#include "../VisualizationPlugin.h"
#include <vector>

// Lissajous / stereo phase plot: Left channel on X axis, Right on Y axis.
// Classic "correlation meter" orientation rotated 45°.
class LissajousPlugin : public VisualizationPlugin
{
public:
    LissajousPlugin();

    juce::String getName() const override { return "Lissajous"; }

    void update(const juce::AudioBuffer<float>& buffer,
                const juce::Array<float>& spectrum) override;

    void render(juce::Graphics& g, juce::Rectangle<int> bounds) override;

private:
    // Snapshot of L/R sample pairs (up to kMaxPoints)
    static constexpr int kMaxPoints = 512;
    std::vector<float> samplesL, samplesR;
    int numPoints = 0;

    // Accumulator image for persistence/trail effect
    juce::Image trailImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LissajousPlugin)
};
