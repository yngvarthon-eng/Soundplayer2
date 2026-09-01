#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

class Equalizer
{
public:
    Equalizer();
    ~Equalizer() = default;

    static constexpr int numBands = 10;

    // Frequency band gains (in dB)
    void setGain(int bandIndex, float gainDb);
    float getGain(int bandIndex) const;

    // Snapshot/restore every band's gain in one call -- used to carry the live
    // UI's settings over to a separate Equalizer instance (e.g. on the video
    // export thread) without sharing state across threads.
    std::array<float, numBands> getAllGains() const;
    void setAllGains(const std::array<float, numBands>& gainsDb);

    void process(juce::AudioBuffer<float>& buffer, int numChannels, int numSamples);
    void prepare(double sampleRate, int blockSize, int numChannels = 2);

private:
    static constexpr float frequencies[numBands] = { 31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
                                                     1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    // Peaking-filter Q for a one-octave bandwidth (RBJ cookbook), matching the
    // octave spacing of `frequencies` above.
    static constexpr float bandQ = 1.4142135f;

    struct Band
    {
        // One filter per output channel -- IIR::Filter keeps single-channel
        // state, so sharing one across channels would leak each channel's
        // history into the other.
        std::vector<juce::dsp::IIR::Filter<float>> channelFilters;
        float gainDb = 0.0f;
    };

    void updateCoefficients(int bandIndex);

    std::array<Band, numBands> bands;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Equalizer)
};
