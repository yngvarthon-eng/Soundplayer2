#include "BarsPeakHoldPlugin.h"
#include <cmath>

BarsPeakHoldPlugin::BarsPeakHoldPlugin()
{
    barHeights.assign(kNumBars, 0.0f);
    peakPos.assign(kNumBars, 0.0f);
    peakVelocity.assign(kNumBars, 0.0f);
    peakHoldCounters.assign(kNumBars, 0);
}

void BarsPeakHoldPlugin::update(const juce::AudioBuffer<float>& /*buffer*/,
                                const juce::Array<float>& spectrum)
{
    int numBins = spectrum.size();
    if (numBins == 0)
        return;

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

        // Peak cap: snaps up instantly when the bar catches up, otherwise
        // holds briefly then free-falls with accelerating gravity.
        float& pp = peakPos[static_cast<size_t>(bar)];
        float& pv = peakVelocity[static_cast<size_t>(bar)];
        int&   pc = peakHoldCounters[static_cast<size_t>(bar)];

        if (h >= pp)
        {
            pp = h;
            pv = 0.0f;
            pc = kPeakHoldTicks;
        }
        else if (pc > 0)
        {
            --pc;
        }
        else
        {
            pv += kGravity;
            pp = juce::jmax(0.0f, pp - pv);
        }
    }
}

void BarsPeakHoldPlugin::render(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.fillAll(juce::Colours::black);

    const float barW = (float)bounds.getWidth() / kNumBars;
    const float maxH = (float)bounds.getHeight();
    const float segH = maxH / (float)kNumSegments;
    const float gap = juce::jmin(2.0f, segH * 0.15f);

    for (int bar = 0; bar < kNumBars; ++bar)
    {
        float h = barHeights[static_cast<size_t>(bar)];
        float x = bounds.getX() + bar * barW;
        int litSegments = (int)std::round(h * kNumSegments);

        for (int seg = 0; seg < kNumSegments; ++seg)
        {
            float segRatio = (float)(seg + 1) / (float)kNumSegments;
            juce::Colour colour;
            if (segRatio > 0.85f)
                colour = juce::Colours::red;
            else if (segRatio > 0.6f)
                colour = juce::Colours::yellow;
            else
                colour = juce::Colours::lime;

            float y = bounds.getBottom() - (seg + 1) * segH;
            bool lit = seg < litSegments;
            g.setColour(lit ? colour : colour.withAlpha(0.08f));
            g.fillRect(x, y + gap * 0.5f, barW - gap, segH - gap);
        }

        // Peak cap segment
        float pp = peakPos[static_cast<size_t>(bar)];
        int peakSegment = juce::jlimit(0, kNumSegments - 1, (int)std::round(pp * kNumSegments) - 1);
        float py = bounds.getBottom() - (peakSegment + 1) * segH;
        g.setColour(juce::Colours::white);
        g.fillRect(x, py + gap * 0.5f, barW - gap, segH - gap);
    }
}
