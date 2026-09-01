#pragma once

#include "../VisualizationPlugin.h"
#include <vector>

// Perspective starfield / tunnel flythrough. Base speed tracks overall
// signal energy; bass hits give a speed and brightness boost, and star hue
// drifts with treble energy.
class StarfieldTunnelPlugin : public VisualizationPlugin
{
public:
    StarfieldTunnelPlugin();

    juce::String getName() const override { return "Starfield"; }

    void update(const juce::AudioBuffer<float>& buffer,
                const juce::Array<float>& spectrum) override;

    void render(juce::Graphics& g, juce::Rectangle<int> bounds) override;

private:
    struct Star
    {
        float x, y;  // in -1..1 space, direction from centre
        float z;     // depth: 1 = far (just spawned), approaches 0 as it nears the viewer
        float hueOffset;
    };

    std::vector<Star> stars;
    juce::Random rng;

    float energy = 0.0f;      // smoothed overall RMS energy
    float bassEnergy = 0.0f;  // smoothed low-band energy, drives speed/brightness kicks
    float trebleEnergy = 0.0f;
    float hueBase = 0.55f;

    static constexpr int kNumStars = 260;

    void resetStar(Star& s, bool randomiseDepth);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StarfieldTunnelPlugin)
};
