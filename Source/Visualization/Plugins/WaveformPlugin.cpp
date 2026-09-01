#include "WaveformPlugin.h"

void WaveformPlugin::update(const juce::AudioBuffer<float>& buffer,
                            const juce::Array<float>& /*spectrum*/)
{
    int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    // Find a rising zero-crossing near the first quarter of the buffer to stabilize display
    auto* data = buffer.getReadPointer(0);
    int triggerOffset = 0;
    int searchEnd = numSamples / 4;
    for (int i = 1; i < searchEnd; ++i)
    {
        if (data[i - 1] < 0.0f && data[i] >= 0.0f)
        {
            triggerOffset = i;
            break;
        }
    }

    int displaySamples = juce::jmin(numSamples - triggerOffset, numSamples * 3 / 4);
    waveformData.resize(static_cast<size_t>(displaySamples));
    for (int i = 0; i < displaySamples; ++i)
        waveformData[static_cast<size_t>(i)] = data[triggerOffset + i];
}

void WaveformPlugin::render(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.fillAll(juce::Colours::black);

    if (waveformData.empty())
        return;

    const float cx = (float)bounds.getX();
    const float cy = (float)bounds.getCentreY();
    const float halfH = (float)bounds.getHeight() * 0.45f;
    const float w = (float)bounds.getWidth();

    // Centre reference line
    g.setColour(juce::Colours::darkgreen.darker(0.4f));
    g.drawHorizontalLine((int)cy, cx, cx + w);

    // Waveform path
    juce::Path path;
    int n = (int)waveformData.size();
    path.startNewSubPath(cx, cy - waveformData[0] * halfH);
    for (int i = 1; i < n; ++i)
    {
        float x = cx + (float)i / (n - 1) * w;
        float y = cy - waveformData[static_cast<size_t>(i)] * halfH;
        path.lineTo(x, y);
    }

    g.setColour(juce::Colours::lime);
    g.strokePath(path, juce::PathStrokeType(1.5f));
}
