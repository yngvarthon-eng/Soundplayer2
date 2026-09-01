#include "SpectrumAnalyzer.h"
#include <cmath>

SpectrumAnalyzer::SpectrumAnalyzer(int size)
    : fftSize(size)
{
    // Compute FFT order such that 2^order == fftSize
    int order = 0;
    int n = fftSize;
    while (n > 1) { n >>= 1; ++order; }

    fft = std::make_unique<juce::dsp::FFT>(order);

    spectrumData.resize(fftSize / 2);
    spectrumData.fill(0.0f);

    // JUCE FFT expects a buffer of size fftSize * 2 (interleaved real + imag)
    fftBuffer.resize(static_cast<size_t>(fftSize) * 2, 0.0f);

    // Pre-compute Hann window coefficients
    hannWindow.resize(static_cast<size_t>(fftSize));
    for (int i = 0; i < fftSize; ++i)
        hannWindow[static_cast<size_t>(i)] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
}

void SpectrumAnalyzer::analyze(const float* audioData, int numSamples)
{
    if (numSamples <= 0 || audioData == nullptr)
        return;

    // Zero the FFT buffer
    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);

    // Copy (and window) up to fftSize samples into the real part of fftBuffer
    int copyCount = juce::jmin(numSamples, fftSize);
    for (int i = 0; i < copyCount; ++i)
        fftBuffer[static_cast<size_t>(i)] = audioData[i] * hannWindow[static_cast<size_t>(i)];

    // Perform in-place forward FFT; result is magnitude spectrum in first fftSize/2 slots
    fft->performFrequencyOnlyForwardTransform(fftBuffer.data());

    // Convert FFT magnitudes to dB and map to 0..1 using an 80 dB dynamic range.
    // Linear normalization (/ fftSize/2) produces values far too small for typical
    // audio; the dB scale spreads the useful range across the full bar height.
    const float normFactor = 2.0f / fftSize; // normalise so full-scale sine = 1.0
    const float floorDb    = -80.0f;
    for (int i = 0; i < fftSize / 2; ++i)
    {
        float magnitude = fftBuffer[static_cast<size_t>(i)] * normFactor;
        float db        = juce::Decibels::gainToDecibels(magnitude, floorDb);
        spectrumData.set(i, juce::jlimit(0.0f, 1.0f, (db - floorDb) / (-floorDb)));
    }
}

void SpectrumAnalyzer::reset()
{
    spectrumData.fill(0.0f);
    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
}
