#include "Equalizer.h"

Equalizer::Equalizer()
{
    for (auto& band : bands)
        band.gainDb = 0.0f;
}

void Equalizer::setGain(int bandIndex, float gainDb)
{
    if (juce::isPositiveAndBelow(bandIndex, numBands))
        bands[bandIndex].gainDb = gainDb;
}

float Equalizer::getGain(int bandIndex) const
{
    if (juce::isPositiveAndBelow(bandIndex, numBands))
        return bands[bandIndex].gainDb;
    return 0.0f;
}

void Equalizer::process(juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
{
    // Simple gain application per band
    for (int i = 0; i < numChannels; ++i)
    {
        auto* channelData = buffer.getWritePointer(i);
        for (int j = 0; j < numSamples; ++j)
        {
            float gainLinear = juce::Decibels::decibelsToGain(bands[0].gainDb);
            channelData[j] *= gainLinear;
        }
    }
}

void Equalizer::prepare(double sampleRate, int blockSize)
{
    // Prepare filters for each band
    for (int i = 0; i < numBands; ++i)
    {
        bands[i].filter.prepare({ sampleRate, (juce::uint32)blockSize, 2 });
    }
}
