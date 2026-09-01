#pragma once

#if defined(SOUNDPLAYER_OPENMPT_SUPPORT) || defined(SOUNDPLAYER_XMP_SUPPORT)

#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
#include <libopenmpt/libopenmpt.hpp>

/**
 * Custom AudioFormatReader that decodes module files (MOD/XM/IT/S3M/MPTM/etc.)
 * via libopenmpt, providing accurate playback with full OpenMPT-extension support.
 */
class OpenMptAudioFormatReader : public juce::AudioFormatReader
{
public:
    OpenMptAudioFormatReader(juce::InputStream* stream, const juce::String& formatName);
    ~OpenMptAudioFormatReader() override;

    bool readSamples(int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                     juce::int64 startSampleInFile, int numSamples) override;

    // Live playback position (called from GUI thread — safe for display purposes)
    int getCurrentRow()     const;
    int getCurrentPattern() const;

private:
    std::unique_ptr<openmpt::module> mod;
    juce::int64 currentSample = 0;

    // Per-channel float decode buffers (non-interleaved)
    std::vector<float> decodeLeft, decodeRight;

    // 48 kHz gives libopenmpt the best internal resampling quality
    static constexpr int kSampleRate  = 48000;
    static constexpr int kNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMptAudioFormatReader)
};
#endif

/**
 * AudioFormat registrar for module tracker files.
 * Register with AudioFormatManager::registerFormat().
 */
class OpenMptAudioFormat : public juce::AudioFormat
{
public:
    OpenMptAudioFormat();

    juce::Array<int> getPossibleSampleRates() override;
    juce::Array<int> getPossibleBitDepths()   override;
    bool canDoStereo()  override { return true; }
    bool canDoMono()    override { return true; }
    bool isCompressed() override { return true; }

    juce::AudioFormatReader* createReaderFor(juce::InputStream* sourceStream,
                                             bool deleteStreamIfOpeningFails) override;
    juce::AudioFormatWriter* createWriterFor(juce::OutputStream*, double, unsigned int,
                                             int, const juce::StringPairArray&, int) override
    {
        return nullptr; // write not supported
    }
};

#endif // SOUNDPLAYER_OPENMPT_SUPPORT || SOUNDPLAYER_XMP_SUPPORT
