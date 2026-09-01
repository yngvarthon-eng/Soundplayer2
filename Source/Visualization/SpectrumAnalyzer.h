#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

class SpectrumAnalyzer
{
public:
    explicit SpectrumAnalyzer(int fftSize = 2048);
    ~SpectrumAnalyzer() = default;

    void analyze(const float* audioData, int numSamples);
    void reset();

    const juce::Array<float>& getSpectrumData() const { return spectrumData; }
    int getFFTSize() const { return fftSize; }

private:
    int fftSize;
    juce::Array<float> spectrumData; // fftSize/2 magnitude bins, normalized 0..1
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftBuffer;    // fftSize * 2 (interleaved real/imag)
    std::vector<float> hannWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
