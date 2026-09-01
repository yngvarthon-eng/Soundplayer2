#include "AudioAnalyzer.h"
#include <juce_events/juce_events.h>
#include <cmath>

AudioAnalyzer::AudioAnalyzer() : juce::Thread("audio-analyzer") {}

AudioAnalyzer::~AudioAnalyzer()
{
    stopThread(2000);
}

void AudioAnalyzer::analyzeFile(const juce::File& file,
                                 juce::AudioFormatManager& formatManager,
                                 std::function<void(Result)> callback)
{
    stopThread(500);
    pendingFile          = file;
    pendingFormatManager = &formatManager;
    pendingCallback      = std::move(callback);
    startThread();
}

void AudioAnalyzer::run()
{
    auto* rawReader = pendingFormatManager->createReaderFor(pendingFile);
    if (rawReader == nullptr || threadShouldExit())
    {
        delete rawReader;
        return;
    }
    std::unique_ptr<juce::AudioFormatReader> reader(rawReader);

    const double      sampleRate  = reader->sampleRate;
    const juce::int64 maxSamples  = juce::jmin(
        reader->lengthInSamples, (juce::int64)(90.0 * sampleRate));

    if (maxSamples <= 0 || threadShouldExit())
        return;

    // ---- Decode up to 90 s, mix to mono ----
    // Use a 2-channel buffer regardless of source channels;
    // AudioFormatReader::read() maps to left/right automatically.
    const int chunkSize = 4096;
    juce::AudioBuffer<float> buf(2, chunkSize);

    // Energy envelope: RMS per 512-sample frame
    const int frameSize = 512;
    std::vector<float> energy;
    energy.reserve((size_t)(maxSamples / frameSize) + 1);

    // Accumulator for the current frame
    float frameSumSq = 0.0f;
    int   frameCount = 0;

    double totalSumSq = 0.0;
    float  peak       = 0.0f;
    juce::int64 totalSamples = 0;

    juce::int64 pos = 0;
    while (pos < maxSamples && !threadShouldExit())
    {
        int toRead = (int)juce::jmin((juce::int64)chunkSize, maxSamples - pos);
        reader->read(&buf, 0, toRead, pos, true, true);
        pos += toRead;

        for (int i = 0; i < toRead && !threadShouldExit(); ++i)
        {
            // Stereo mix-down to mono
            float s = (buf.getSample(0, i) + buf.getSample(1, i)) * 0.5f;

            float absSample = std::abs(s);
            if (absSample > peak) peak = absSample;
            totalSumSq += (double)(s * s);
            ++totalSamples;

            // Accumulate into energy frame
            frameSumSq += s * s;
            ++frameCount;
            if (frameCount == frameSize)
            {
                energy.push_back(std::sqrt(frameSumSq / frameSize));
                frameSumSq = 0.0f;
                frameCount = 0;
            }
        }
    }

    if (threadShouldExit() || totalSamples == 0)
        return;

    Result result;
    result.valid  = true;
    result.peakDb = peak > 0.0f
                    ? juce::Decibels::gainToDecibels(peak)
                    : -100.0f;
    result.rmsDb  = totalSumSq > 0.0
                    ? juce::Decibels::gainToDecibels(
                          (float)std::sqrt(totalSumSq / (double)totalSamples))
                    : -100.0f;

    const double frameRate = sampleRate / frameSize;
    result.bpm = detectBpm(energy, frameRate);

    auto cb = pendingCallback;
    juce::MessageManager::callAsync([cb, result]() { cb(result); });
}

float AudioAnalyzer::detectBpm(const std::vector<float>& energy, double frameRate)
{
    // Need at least ~2 seconds of envelope to detect 60 BPM
    if (energy.size() < 64 || threadShouldExit())
        return 0.0f;

    // Cap at 8000 frames to bound computation time
    size_t n = std::min(energy.size(), (size_t)8000);

    // Lag range for BPM [55, 210]
    int lagMin = std::max(1, (int)(frameRate * 60.0 / 210.0));
    int lagMax = std::min((int)(n / 2), (int)(frameRate * 60.0 / 55.0));

    if (lagMin >= lagMax)
        return 0.0f;

    // Normalised autocorrelation: R(lag) / R(0)
    double r0 = 0.0;
    for (size_t i = 0; i < n; ++i)
        r0 += energy[i] * energy[i];

    if (r0 == 0.0)
        return 0.0f;

    float bestCorr = -1.0f;
    int   bestLag  = lagMin;

    for (int lag = lagMin; lag <= lagMax && !threadShouldExit(); ++lag)
    {
        double corr = 0.0;
        int    cnt  = (int)n - lag;
        for (int i = 0; i < cnt; ++i)
            corr += energy[(size_t)i] * energy[(size_t)(i + lag)];
        corr /= (double)cnt;
        float normCorr = (float)(corr / r0 * (double)n);

        if (normCorr > bestCorr)
        {
            bestCorr = normCorr;
            bestLag  = lag;
        }
    }

    if (threadShouldExit())
        return 0.0f;

    float bpm = (float)(frameRate * 60.0 / bestLag);

    // Octave disambiguation: if result < 90 BPM, check whether doubling is plausible
    if (bpm < 90.0f && bestLag / 2 >= lagMin)
    {
        int halfLag = bestLag / 2;
        double corr = 0.0;
        int cnt = (int)n - halfLag;
        for (int i = 0; i < cnt; ++i)
            corr += energy[(size_t)i] * energy[(size_t)(i + halfLag)];
        corr /= (double)cnt;
        float normCorr = (float)(corr / r0 * (double)n);
        if (normCorr > bestCorr * 0.8f)
            bpm *= 2.0f;
    }

    return bpm;
}
