#include "RadialSpectrumPlugin.h"
#include <cmath>

RadialSpectrumPlugin::RadialSpectrumPlugin()
{
    barHeights.assign(kNumBars, 0.0f);
}

void RadialSpectrumPlugin::update(const juce::AudioBuffer<float>& buffer,
                                  const juce::Array<float>& spectrum)
{
    int numBins = spectrum.size();
    if (numBins > 0)
    {
        // Log-space mapping onto kNumBars bars, same approach as SpectrumBarsPlugin.
        for (int bar = 0; bar < kNumBars; ++bar)
        {
            float t0 = (float)bar / kNumBars;
            float t1 = (float)(bar + 1) / kNumBars;
            int binStart = juce::jmax(1, (int)(std::pow(numBins, t0)));
            int binEnd   = juce::jmax(binStart + 1, (int)(std::pow(numBins, t1)));
            binEnd = juce::jmin(binEnd, numBins);

            float peak = 0.0f;
            for (int b = binStart; b < binEnd; ++b)
                peak = juce::jmax(peak, spectrum[b]);

            float& h = barHeights[static_cast<size_t>(bar)];
            if (peak > h)
                h = h + (peak - h) * kRiseRate;
            else
                h = juce::jmax(0.0f, h - kDecayRate);
        }
    }

    // Overall energy from the raw waveform, for the centre disc pulse.
    int n = buffer.getNumSamples();
    if (n > 0)
    {
        auto* data = buffer.getReadPointer(0);
        float sumSq = 0.0f;
        for (int i = 0; i < n; ++i)
            sumSq += data[i] * data[i];
        float rms = std::sqrt(sumSq / (float)n);
        float target = juce::jlimit(0.0f, 1.0f, rms * 4.0f);
        energy += (target - energy) * 0.2f;
    }

    rotation += 0.003f + energy * 0.01f;
    if (rotation > juce::MathConstants<float>::twoPi)
        rotation -= juce::MathConstants<float>::twoPi;
}

void RadialSpectrumPlugin::render(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.fillAll(juce::Colours::black);

    const float cx = (float)bounds.getCentreX();
    const float cy = (float)bounds.getCentreY();
    const float outerLimit = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.48f;
    const float innerRadius = outerLimit * (0.22f + energy * 0.08f);
    const float maxBarLen = outerLimit - innerRadius;

    if (maxBarLen <= 0.0f)
        return;

    // Centre disc, pulsing with overall energy.
    g.setColour(juce::Colour(0xff2a7abf).withAlpha(0.25f + energy * 0.4f));
    g.fillEllipse(cx - innerRadius, cy - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

    for (int bar = 0; bar < kNumBars; ++bar)
    {
        float h = barHeights[static_cast<size_t>(bar)];
        float angle = rotation + (juce::MathConstants<float>::twoPi * (float)bar / (float)kNumBars);

        float r0 = innerRadius;
        float r1 = innerRadius + h * maxBarLen;

        float dx = std::cos(angle);
        float dy = std::sin(angle);

        float x0 = cx + dx * r0;
        float y0 = cy + dy * r0;
        float x1 = cx + dx * r1;
        float y1 = cy + dy * r1;

        // Colour cycles smoothly around the ring, brighter at the tip.
        float hue = (float)bar / (float)kNumBars;
        juce::Colour colour = juce::Colour::fromHSV(hue, 0.85f, 0.5f + h * 0.5f, 1.0f);

        g.setColour(colour);
        g.drawLine(x0, y0, x1, y1, 2.5f);
    }

    // Thin outline ring at the base radius for definition.
    g.setColour(juce::Colours::darkgrey.withAlpha(0.6f));
    g.drawEllipse(cx - innerRadius, cy - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);
}
