#include "ParticleBloomPlugin.h"
#include <algorithm>
#include <cmath>

ParticleBloomPlugin::ParticleBloomPlugin()
{
    particles.reserve(kMaxParticles);
}

float ParticleBloomPlugin::bandEnergy(const juce::Array<float>& spectrum, float loFrac, float hiFrac)
{
    int numBins = spectrum.size();
    if (numBins == 0)
        return 0.0f;

    int binStart = juce::jmax(1, (int)(loFrac * numBins));
    int binEnd   = juce::jmin(numBins, juce::jmax(binStart + 1, (int)(hiFrac * numBins)));

    float peak = 0.0f;
    for (int b = binStart; b < binEnd; ++b)
        peak = juce::jmax(peak, spectrum[b]);
    return peak;
}

void ParticleBloomPlugin::spawnBurst(float bandEnergyVal, float baseHue, float originX, int count)
{
    for (int i = 0; i < count && particles.size() < (size_t)kMaxParticles; ++i)
    {
        Particle p;
        p.x = originX + rng.nextFloat() * 0.06f - 0.03f;
        p.y = 1.0f;

        float angle = -juce::MathConstants<float>::halfPi
                      + (rng.nextFloat() - 0.5f) * juce::MathConstants<float>::pi * 0.7f;
        float speed = 0.006f + bandEnergyVal * 0.02f * rng.nextFloat();
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;

        p.lifeSpan = 0.6f + rng.nextFloat() * 0.6f;
        p.life = p.lifeSpan;
        p.hue = baseHue + (rng.nextFloat() - 0.5f) * 0.06f;
        p.size = 2.0f + bandEnergyVal * 10.0f * rng.nextFloat();

        particles.push_back(p);
    }
}

void ParticleBloomPlugin::update(const juce::AudioBuffer<float>& /*buffer*/,
                                 const juce::Array<float>& spectrum)
{
    float bass   = bandEnergy(spectrum, 0.0f, 0.08f);
    float mid    = bandEnergy(spectrum, 0.08f, 0.35f);
    float treble = bandEnergy(spectrum, 0.35f, 1.0f);

    bassEnergy   += (bass - bassEnergy) * 0.4f;
    midEnergy    += (mid - midEnergy) * 0.3f;
    trebleEnergy += (treble - trebleEnergy) * 0.3f;

    if (bassEnergy > 0.35f)
        spawnBurst(bassEnergy, 0.02f /* red-orange */, 0.5f, (int)(bassEnergy * 10.0f));
    if (midEnergy > 0.3f)
        spawnBurst(midEnergy, 0.33f /* green */, rng.nextFloat(), (int)(midEnergy * 6.0f));
    if (trebleEnergy > 0.25f)
        spawnBurst(trebleEnergy, 0.6f /* blue-violet */, rng.nextFloat(), (int)(trebleEnergy * 5.0f));

    // Advance and cull particles.
    for (auto& p : particles)
    {
        p.x += p.vx;
        p.y += p.vy;
        p.vy += 0.00015f; // gentle gravity drift
        p.life -= 1.0f / 60.0f;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
                                    [](const Particle& p) { return p.life <= 0.0f; }),
                     particles.end());
}

void ParticleBloomPlugin::render(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Trail-free black background; particles carry their own fade via alpha.
    g.fillAll(juce::Colours::black);

    const float w = (float)bounds.getWidth();
    const float h = (float)bounds.getHeight();
    if (w <= 0.0f || h <= 0.0f)
        return;

    for (const auto& p : particles)
    {
        float t = juce::jlimit(0.0f, 1.0f, p.life / p.lifeSpan);
        float px = bounds.getX() + p.x * w;
        float py = bounds.getY() + p.y * h;
        float size = p.size * (0.4f + 0.6f * t);

        juce::Colour colour = juce::Colour::fromHSV(p.hue, 0.9f, 1.0f, t);
        g.setColour(colour);
        g.fillEllipse(px - size * 0.5f, py - size * 0.5f, size, size);
    }
}
