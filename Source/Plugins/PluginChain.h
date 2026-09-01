#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

/**
 * An ordered chain of AudioPluginInstances applied in series on the audio thread.
 *
 * Thread safety:
 *   - process() is called from the audio thread.
 *   - addPlugin/removePlugin/movePlugin are called from the GUI thread.
 *   - Modifications take effect at the next audio callback via an atomic swap.
 */
class PluginChain
{
public:
    PluginChain() = default;
    ~PluginChain() = default;

    // Called from audio thread before playback starts.
    void prepare(double sampleRate, int maxBlockSize);

    // Called from audio thread each callback — processes buffer in place.
    void process(juce::AudioBuffer<float>& buffer);

    // GUI-thread mutations — each call acquires the lock briefly.
    void addPlugin(std::unique_ptr<juce::AudioPluginInstance> instance);
    void removePlugin(int index);
    void movePlugin(int fromIndex, int toIndex);
    void setBypass(int index, bool bypassed);

    // GUI-thread read-only queries — acquire lock.
    int size() const;
    juce::AudioPluginInstance* getPlugin(int index) const;
    bool isBypassed(int index) const;
    juce::String getPluginName(int index) const;

private:
    struct Entry
    {
        std::unique_ptr<juce::AudioPluginInstance> instance;
        bool bypassed = false;
    };

    mutable juce::CriticalSection lock;
    std::vector<Entry> plugins;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChain)
};
