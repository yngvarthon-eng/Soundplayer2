#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <vector>

/**
 * Analyzes an audio file in a background thread, computing:
 *   - Tempo (BPM) via energy-envelope autocorrelation
 *   - RMS loudness (dBFS)
 *   - True peak (dBFS)
 *
 * Usage: call analyzeFile() — the callback fires on the message thread when done.
 */
class AudioAnalyzer : public juce::Thread
{
public:
    struct Result
    {
        float bpm    = 0.0f;    // 0 = could not detect
        float rmsDb  = -100.0f; // dBFS
        float peakDb = -100.0f; // dBFS
        bool  valid  = false;
    };

    AudioAnalyzer();
    ~AudioAnalyzer() override;

    /** Starts background analysis. Cancels any in-progress analysis first. */
    void analyzeFile(const juce::File& file,
                     juce::AudioFormatManager& formatManager,
                     std::function<void(Result)> callback);

    // juce::Thread
    void run() override;

private:
    juce::File                   pendingFile;
    juce::AudioFormatManager*    pendingFormatManager = nullptr;
    std::function<void(Result)>  pendingCallback;

    /** Autocorrelation-based BPM detection on a mono energy envelope. */
    float detectBpm(const std::vector<float>& energy, double frameRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioAnalyzer)
};
