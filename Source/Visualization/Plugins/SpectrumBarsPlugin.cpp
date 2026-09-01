#include "SpectrumBarsPlugin.h"
#include <cmath>

SpectrumBarsPlugin::SpectrumBarsPlugin()
{
    barHeights.assign(kNumBars, 0.0f);
    peakHeights.assign(kNumBars, 0.0f);
    peakHoldCounters.assign(kNumBars, 0);
}

void SpectrumBarsPlugin::update(const juce::AudioBuffer<float>& /*buffer*/,
                                const juce::Array<float>& spectrum)
{
    int numBins = spectrum.size();
    if (numBins == 0)
        return;

    // Map kNumBars log-spaced bars onto FFT bins
    // Bin 0 is DC; useful range is roughly bins 1..numBins-1
    for (int bar = 0; bar < kNumBars; ++bar)
    {
        // Log-space mapping: bar 0 -> low bins, bar kNumBars-1 -> high bins
        float t0 = (float)bar / kNumBars;
        float t1 = (float)(bar + 1) / kNumBars;
        int binStart = juce::jmax(1, (int)(std::pow(numBins, t0)));
        int binEnd   = juce::jmax(binStart + 1, (int)(std::pow(numBins, t1)));
        binEnd = juce::jmin(binEnd, numBins);

        float peak = 0.0f;
        for (int b = binStart; b < binEnd; ++b)
            peak = juce::jmax(peak, spectrum[b]);

        // Smooth: rise fast, decay slow
        float& h = barHeights[static_cast<size_t>(bar)];
        if (peak > h)
            h = h + (peak - h) * kRiseRate;
        else
            h = juce::jmax(0.0f, h - kDecayRate);

        // Peak hold
        float& ph = peakHeights[static_cast<size_t>(bar)];
        int&   pc = peakHoldCounters[static_cast<size_t>(bar)];
        if (h >= ph)
        {
            ph = h;
            pc = kPeakHoldTicks;
        }
        else if (pc > 0)
        {
            --pc;
        }
        else
        {
            ph = juce::jmax(0.0f, ph - kDecayRate * 0.5f);
        }
    }
}

void SpectrumBarsPlugin::render(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.fillAll(juce::Colours::black);

    const float barW = (float)bounds.getWidth() / kNumBars;
    const float maxH = (float)bounds.getHeight();

    for (int bar = 0; bar < kNumBars; ++bar)
    {
        float h = barHeights[static_cast<size_t>(bar)] * maxH;
        float x = bounds.getX() + bar * barW;
        float y = bounds.getBottom() - h;

        // Color gradient by height
        juce::Colour colour;
        float ratio = h / maxH;
        if (ratio > 0.7f)
            colour = juce::Colours::red;
        else if (ratio > 0.4f)
            colour = juce::Colours::yellow;
        else
            colour = juce::Colours::lime;

        g.setColour(colour);
        g.fillRect(x, y, barW - 1.0f, h);

        // Peak hold tick
        float ph = peakHeights[static_cast<size_t>(bar)] * maxH;
        if (ph > 2.0f)
        {
            g.setColour(juce::Colours::white);
            g.fillRect(x, bounds.getBottom() - ph - 1.0f, barW - 1.0f, 2.0f);
        }
    }
}
