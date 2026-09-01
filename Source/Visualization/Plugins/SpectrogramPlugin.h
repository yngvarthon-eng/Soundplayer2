#pragma once

#include "../VisualizationPlugin.h"

// Scrolling waterfall spectrogram: time on X axis (newest on right), frequency on Y axis.
class SpectrogramPlugin : public VisualizationPlugin
{
public:
    SpectrogramPlugin();

    juce::String getName() const override { return "Spectrogram"; }

    void update(const juce::AudioBuffer<float>& buffer,
                const juce::Array<float>& spectrum) override;

    void render(juce::Graphics& g, juce::Rectangle<int> bounds) override;

private:
    // Internal image painted column by column.  Resized lazily when bounds change.
    juce::Image image;
    int imageWritePos = 0; // next column to write (wraps around for circular buffer)
    bool imageReady = false;

    juce::Array<float> latestSpectrum;

    static juce::Colour spectrumToColour(float magnitude);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramPlugin)
};
