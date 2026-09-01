#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

class Equalizer
{
public:
    Equalizer();
    ~Equalizer() = default;

    // Frequency band gains (in dB)
    void setGain(int bandIndex, float gainDb);
    float getGain(int bandIndex) const;

    void process(juce::AudioBuffer<float>& buffer, int numChannels, int numSamples);
    void prepare(double sampleRate, int blockSize);

private:
    enum { numBands = 10 };
    static constexpr float frequencies[numBands] = { 31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
                                                     1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    
    struct Band
    {
        juce::dsp::IIR::Filter<float> filter;
        float gainDb = 0.0f;
    };

    std::array<Band, numBands> bands;
    juce::dsp::ProcessorChain<> chain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Equalizer)
};
