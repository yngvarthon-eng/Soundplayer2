#include "StarfieldTunnelPlugin.h"
#include <cmath>

StarfieldTunnelPlugin::StarfieldTunnelPlugin()
{
    stars.resize(kNumStars);
    for (auto& s : stars)
        resetStar(s, true);
}

void StarfieldTunnelPlugin::resetStar(Star& s, bool randomiseDepth)
{
    // Random direction, biased away from dead centre so stars don't bunch up there.
    float angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
    float radius = 0.15f + rng.nextFloat() * 0.85f;
    s.x = std::cos(angle) * radius;
    s.y = std::sin(angle) * radius;
    s.z = randomiseDepth ? rng.nextFloat() : 1.0f;
    s.hueOffset = rng.nextFloat() * 0.08f - 0.04f;
}

void StarfieldTunnelPlugin::update(const juce::AudioBuffer<float>& buffer,
                                   const juce::Array<float>& spectrum)
{
    // Overall RMS energy from the raw waveform.
    int n = buffer.getNumSamples();
    if (n > 0)
    {
        auto* data = buffer.getReadPointer(0);
        float sumSq = 0.0f;
        for (int i = 0; i < n; ++i)
            sumSq += data[i] * data[i];
        float rms = std::sqrt(sumSq / (float)n);
        energy += (juce::jlimit(0.0f, 1.0f, rms * 4.0f) - energy) * 0.15f;
    }

    int numBins = spectrum.size();
    if (numBins > 0)
    {
        int bassEnd = juce::jmax(2, (int)(numBins * 0.08f));
        float bass = 0.0f;
        for (int b = 1; b < bassEnd; ++b)
            bass = juce::jmax(bass, spectrum[b]);
        bassEnergy += (bass - bassEnergy) * 0.3f;

        int trebleStart = (int)(numBins * 0.5f);
        float treble = 0.0f;
        for (int b = trebleStart; b < numBins; ++b)
            treble = juce::jmax(treble, spectrum[b]);
        trebleEnergy += (treble - trebleEnergy) * 0.2f;
    }

    hueBase += 0.0006f + trebleEnergy * 0.002f;
    if (hueBase > 1.0f) hueBase -= 1.0f;

    // Speed: idle drift plus energy and bass-kick contributions.
    float speed = 0.004f + energy * 0.02f + bassEnergy * bassEnergy * 0.05f;

    for (auto& s : stars)
    {
        s.z -= speed;
        if (s.z <= 0.02f)
            resetStar(s, false);
    }
}

void StarfieldTunnelPlugin::render(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.fillAll(juce::Colours::black);

    const float cx = (float)bounds.getCentreX();
    const float cy = (float)bounds.getCentreY();
    const float scale = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    for (const auto& s : stars)
    {
        float invZ = 1.0f / s.z;
        float px = cx + s.x * invZ * scale;
        float py = cy + s.y * invZ * scale;

        if (px < bounds.getX() - 4 || px > bounds.getRight() + 4
            || py < bounds.getY() - 4 || py > bounds.getBottom() + 4)
            continue;

        // Nearer stars (small z) are bigger and brighter; a bass hit boosts brightness.
        float closeness = 1.0f - s.z;
        float size = 1.0f + closeness * closeness * 6.0f;
        float brightness = juce::jlimit(0.0f, 1.0f, 0.3f + closeness * 0.7f + bassEnergy * 0.3f);

        juce::Colour colour = juce::Colour::fromHSV(hueBase + s.hueOffset, 0.5f, brightness, 1.0f);
        g.setColour(colour);
        g.fillEllipse(px - size * 0.5f, py - size * 0.5f, size, size);

        // Streak trailing back toward centre for the closest, fastest-moving stars.
        if (closeness > 0.6f)
        {
            float trailZ = s.z + 0.04f;
            float tpx = cx + s.x * (1.0f / trailZ) * scale;
            float tpy = cy + s.y * (1.0f / trailZ) * scale;
            g.setColour(colour.withAlpha(0.35f));
            g.drawLine(px, py, tpx, tpy, size * 0.5f);
        }
    }
}
