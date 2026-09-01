#pragma once

#include "../VisualizationPlugin.h"
#include <vector>

// Bass/mid/treble-reactive particle burst. Each frequency band spawns
// particles of its own colour and direction when its energy exceeds a
// threshold; particles drift, fade, and shrink over their lifetime.
class ParticleBloomPlugin : public VisualizationPlugin
{
public:
    ParticleBloomPlugin();

    juce::String getName() const override { return "Particle Bloom"; }

    void update(const juce::AudioBuffer<float>& buffer,
                const juce::Array<float>& spectrum) override;

    void render(juce::Graphics& g, juce::Rectangle<int> bounds) override;

private:
    struct Particle
    {
        float x, y;       // position, normalised 0..1 relative to render bounds
        float vx, vy;      // velocity in normalised units / frame
        float life;        // 1..0, remaining lifetime
        float lifeSpan;     // initial life, for size/alpha scaling
        float hue;
        float size;
    };

    std::vector<Particle> particles;
    juce::Random rng;

    // Smoothed per-band energy, used to detect onsets and to size new particles.
    float bassEnergy = 0.0f, midEnergy = 0.0f, trebleEnergy = 0.0f;

    static constexpr int kMaxParticles = 400;

    static float bandEnergy(const juce::Array<float>& spectrum, float loFrac, float hiFrac);
    void spawnBurst(float bandEnergyVal, float baseHue, float originX, int count);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleBloomPlugin)
};
