#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Analysis/AudioAnalyzer.h"

/**
 * Compact panel that displays BPM, RMS, and true-peak analysis for the current track.
 * Call analyzeFile() when a new file track is loaded; call clear() for streams.
 */
class AudioAnalysisComponent : public juce::Component
{
public:
    explicit AudioAnalysisComponent();
    ~AudioAnalysisComponent() override = default;

    /** Start background analysis of a file track. */
    void analyzeFile(const juce::File& file);

    /** Cancel any running analysis and reset the display. */
    void clear();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::String             sourceInfo;
    // Self-contained format manager (registered with basic formats)
    juce::AudioFormatManager formatManager;

    AudioAnalyzer            analyzer;
    AudioAnalyzer::Result    lastResult;
    bool                     analyzing = false;
    juce::File               currentFile;

    juce::TextButton analyzeBtn { "Analyze" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioAnalysisComponent)
};
