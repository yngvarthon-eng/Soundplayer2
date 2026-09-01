#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

class FormatManager
{
public:
    FormatManager();
    ~FormatManager() = default;

    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    
    bool isSupportedFormat(const juce::File& file) const;
    juce::String getSupportedFormats() const;

private:
    juce::AudioFormatManager formatManager;
};
