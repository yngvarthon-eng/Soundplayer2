#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

class MultiChannelAudioFormatReaderSource : public juce::PositionableAudioSource
{
public:
    MultiChannelAudioFormatReaderSource(juce::AudioFormatReader* sourceReader,
                                        bool deleteReaderWhenThisIsDeleted);
    ~MultiChannelAudioFormatReaderSource() override;

    void setLooping(bool shouldLoop) override;
    bool isLooping() const override { return looping; }

    juce::AudioFormatReader* getAudioFormatReader() const noexcept { return reader; }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;

    void setNextReadPosition(juce::int64 newPosition) override;
    juce::int64 getNextReadPosition() const override;
    juce::int64 getTotalLength() const override;

private:
    juce::OptionalScopedPointer<juce::AudioFormatReader> reader;
    juce::int64 nextPlayPos = 0;
    bool looping = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiChannelAudioFormatReaderSource)
};