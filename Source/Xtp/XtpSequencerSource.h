#pragma once

#include "XtpInstrumentVoice.h"
#include "XtpSequencer.h"
#include "XtpSong.h"
#include "XtpTransportClock.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <map>
#include <memory>

// Live-sequenced playback source for a parsed .xtp song. Unlike
// MultiChannelAudioFormatReaderSource (which pulls fixed PCM from a
// juce::AudioFormatReader), this actively drives exTracker's row/tick clock
// and mixes whichever instrument voices are triggered along the way -- there
// is no underlying decodable stream to seek/read.
//
// Slots into AudioEngine exactly where MultiChannelAudioFormatReaderSource
// does (juce::AudioTransportSource::setSource()), so play/pause/stop/seek and
// the whole downstream EQ/plugin-chain/recording pipeline are reused unchanged.
class XtpSequencerSource : public juce::PositionableAudioSource, private xtp::NoteSink
{
public:
    XtpSequencerSource(const juce::File& file, double outputSampleRate);
    ~XtpSequencerSource() override;

    // Fixed internal render rate (matches ModPlugFormat's OpenMptAudioFormatReader
    // convention of always declaring a fixed rate to juce::AudioTransportSource::
    // setSource() and letting its own resampler handle the device's real rate).
    static constexpr double kRenderSampleRate = 48000.0;

    bool isValid() const { return song.loaded && !song.patterns.empty(); }
    const juce::String& getLoadError() const { return loadError; }
    const xtp::Song& getSong() const { return song; }

    // juce::PositionableAudioSource
    void setNextReadPosition(juce::int64 newPosition) override;
    juce::int64 getNextReadPosition() const override { return position; }
    juce::int64 getTotalLength() const override { return totalLengthSamples; }
    void setLooping(bool shouldLoop) override { looping = shouldLoop; }
    bool isLooping() const override { return looping; }

    // juce::AudioSource
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

    // Live playback position for AudioEngine's UI poll. -1 if not yet dispatched.
    int getCurrentRow() const { return currentRowForUi.load(); }
    int getCurrentPatternIndex() const { return currentPatternForUi.load(); }

private:
    // xtp::NoteSink -- resolves (instrument, sample) to a voice using the same
    // priority order as exTracker's PluginHost::triggerNoteOnResolved.
    void noteOn(std::uint8_t instrument, std::uint16_t sample, int midiNote,
               std::uint8_t velocity, bool retrigger) override;
    void noteOff(std::uint8_t instrument, std::uint16_t sample, int midiNote) override;

    xtp::InstrumentVoice* resolveVoice(std::uint8_t instrument, std::uint16_t sample);
    void buildInstrumentVoices();
    void seekToSampleZero();
    void advanceOneTick();
    void advanceSongOrder();
    void renderChunk(int destOffset, int numSamples);

    xtp::Song song;
    juce::String loadError;
    double sampleRate = kRenderSampleRate;
    bool looping = false;
    bool finished = false;
    bool suppressAudibleDispatch = false; // true while fast-forwarding through a seek

    xtp::TransportClock transport;
    xtp::Sequencer sequencer;

    std::array<std::unique_ptr<xtp::InstrumentVoice>, 16> instrumentVoices;
    std::map<int, std::unique_ptr<xtp::InstrumentVoice>> sampleVoices;

    int songOrderPosition = 0;
    int currentPatternIndex = 0;

    double tickTimeAccumulator = 0.0;
    juce::int64 totalLengthSamples = 0;
    juce::int64 position = 0;

    std::atomic<int> currentRowForUi { -1 };
    std::atomic<int> currentPatternForUi { -1 };

    std::vector<float> monoScratch;
    std::vector<float> voiceScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XtpSequencerSource)
};
