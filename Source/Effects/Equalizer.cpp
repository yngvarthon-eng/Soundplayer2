#include "Equalizer.h"

Equalizer::Equalizer()
{
    for (auto& band : bands)
        band.gainDb = 0.0f;
}

void Equalizer::setGain(int bandIndex, float gainDb)
{
    if (juce::isPositiveAndBelow(bandIndex, numBands))
    {
        bands[(size_t) bandIndex].gainDb = gainDb;
        updateCoefficients(bandIndex);
    }
}

float Equalizer::getGain(int bandIndex) const
{
    if (juce::isPositiveAndBelow(bandIndex, numBands))
        return bands[(size_t) bandIndex].gainDb;
    return 0.0f;
}

std::array<float, Equalizer::numBands> Equalizer::getAllGains() const
{
    std::array<float, numBands> gains {};
    for (int i = 0; i < numBands; ++i)
        gains[(size_t) i] = bands[(size_t) i].gainDb;
    return gains;
}

void Equalizer::setAllGains(const std::array<float, numBands>& gainsDb)
{
    for (int i = 0; i < numBands; ++i)
    {
        bands[(size_t) i].gainDb = gainsDb[(size_t) i];
        updateCoefficients(i);
    }
}

void Equalizer::updateCoefficients(int bandIndex)
{
    auto& band = bands[(size_t) bandIndex];
    if (band.channelFilters.empty())
        return; // not prepared yet -- prepare() applies the current gain then

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate, frequencies[bandIndex], bandQ,
        juce::Decibels::decibelsToGain(band.gainDb));

    for (auto& filter : band.channelFilters)
        filter.coefficients = coeffs;
}

void Equalizer::process(juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (ch >= (int) bands[0].channelFilters.size())
            break; // prepare() hasn't been called for this many channels

        auto* channelData = buffer.getWritePointer(ch);
        juce::dsp::AudioBlock<float> block(&channelData, 1, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> context(block);

        for (auto& band : bands)
            band.channelFilters[(size_t) ch].process(context);
    }
}

void Equalizer::prepare(double sampleRate, int blockSize, int numChannels)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    numChannels = juce::jmax(1, numChannels);

    const juce::dsp::ProcessSpec spec { currentSampleRate, (juce::uint32) blockSize, 1 };

    for (auto& band : bands)
    {
        band.channelFilters.clear();
        band.channelFilters.resize((size_t) numChannels);
        for (auto& filter : band.channelFilters)
            filter.prepare(spec);
    }

    for (int i = 0; i < numBands; ++i)
        updateCoefficients(i);
}
