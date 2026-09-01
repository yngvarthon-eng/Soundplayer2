#include "VUMeterPlugin.h"
#include <cmath>

VUMeterPlugin::VUMeterPlugin() = default;

float VUMeterPlugin::rmsToDb(const float* data, int numSamples)
{
    if (numSamples <= 0)
        return -60.0f;
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        sum += data[i] * data[i];
    float rms = std::sqrt(sum / numSamples);
    if (rms < 1e-10f)
        return -60.0f;
    return juce::jmax(-60.0f, 20.0f * std::log10(rms));
}

void VUMeterPlugin::update(const juce::AudioBuffer<float>& buffer,
                           const juce::Array<float>& /*spectrum*/)
{
    int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    float newL = rmsToDb(buffer.getReadPointer(0), numSamples);
    float newR = (buffer.getNumChannels() > 1)
               ? rmsToDb(buffer.getReadPointer(1), numSamples)
               : newL;

    // Smooth: rise fast, decay slowly
    auto smooth = [this](float& level, float target) {
        if (target > level)
            level = level + (target - level) * 0.4f;
        else
            level = juce::jmax(-60.0f, level - kDecayRate * 60.0f);
    };
    smooth(levelL, newL);
    smooth(levelR, newR);

    // Peak hold — L
    if (levelL >= peakL) { peakL = levelL; peakCounterL = kPeakHoldTicks; }
    else if (peakCounterL > 0) { --peakCounterL; }
    else peakL = juce::jmax(-60.0f, peakL - 0.5f);

    // Peak hold — R
    if (levelR >= peakR) { peakR = levelR; peakCounterR = kPeakHoldTicks; }
    else if (peakCounterR > 0) { --peakCounterR; }
    else peakR = juce::jmax(-60.0f, peakR - 0.5f);
}

// Maps dBFS (-60..0) to normalised bar fraction 0..1
static float dbToFraction(float db)
{
    return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

void VUMeterPlugin::drawBar(juce::Graphics& g, juce::Rectangle<int> rect,
                            float levelDb, float peakDb) const
{
    const int barH = rect.getHeight();
    const int barX = rect.getX();
    const int barY = rect.getY();
    const int barW = rect.getWidth();

    // Background
    g.setColour(juce::Colours::black);
    g.fillRect(rect);

    // Segmented bar drawn from bottom up
    float frac = dbToFraction(levelDb);
    int   filledH = (int)(frac * barH);

    for (int y = barH - 1; y >= barH - filledH; --y)
    {
        float yFrac = (float)(barH - y) / barH; // 0 at top, 1 at bottom
        juce::Colour c;
        if (yFrac < 0.15f)      c = juce::Colours::red;
        else if (yFrac < 0.35f) c = juce::Colours::yellow;
        else                    c = juce::Colours::lime;

        g.setColour(c);
        g.fillRect(barX, barY + y, barW, 1);
    }

    // Peak tick
    float peakFrac = dbToFraction(peakDb);
    int peakY = barY + barH - (int)(peakFrac * barH);
    peakY = juce::jlimit(barY, barY + barH - 2, peakY);
    g.setColour(juce::Colours::white);
    g.fillRect(barX, peakY, barW, 2);

    // dBFS scale markers
    g.setColour(juce::Colours::grey);
    g.setFont(9.0f);
    for (int db : { 0, -6, -12, -24, -48 })
    {
        float f = dbToFraction((float)db);
        int gy = barY + barH - (int)(f * barH);
        g.drawHorizontalLine(gy, (float)barX - 14, (float)barX);
        g.drawText(juce::String(db), barX - 28, gy - 5, 26, 10,
                   juce::Justification::right, false);
    }
}

void VUMeterPlugin::render(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.fillAll(juce::Colours::black.brighter(0.05f));

    const int labelW = 32; // left margin for dB labels
    const int gap    = 8;
    const int barW   = juce::jmax(20, (bounds.getWidth() - labelW - gap * 3) / 2);
    const int barH   = bounds.getHeight() - 20;
    const int barY   = bounds.getY() + 10;

    int xL = bounds.getX() + labelW + gap;
    int xR = xL + barW + gap;

    drawBar(g, { xL, barY, barW, barH }, levelL, peakL);
    drawBar(g, { xR, barY, barW, barH }, levelR, peakR);

    // Channel labels
    g.setColour(juce::Colours::white);
    g.setFont(11.0f);
    g.drawText("L", xL, bounds.getBottom() - 16, barW, 14, juce::Justification::centred);
    g.drawText("R", xR, bounds.getBottom() - 16, barW, 14, juce::Justification::centred);
}
