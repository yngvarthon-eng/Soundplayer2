#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "MultiChannelAudioFormatReaderSource.h"
#include "../Effects/Equalizer.h"
#include "../Visualization/ModulePatternData.h"
#include "../Plugins/PluginChain.h"
#include "../Xtp/XtpSequencerSource.h"

class PlaylistManager;
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
class OpenMptAudioFormatReader;
#endif

class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::ChangeListener
{
public:
    enum class StereoExpansionMode
    {
        Native = 0,
        MultiSpeakerStereo
    };

    AudioEngine(PlaylistManager* playlistMgr);
    ~AudioEngine();

    // Builds a format manager registered exactly like the one this class uses for
    // playback (basic formats + OpenMpt when compiled in). Used by code that needs
    // to decode a file independently of the live playback engine (e.g. VideoExporter).
    static std::unique_ptr<juce::AudioFormatManager> createStandaloneFormatManager();

    // Call once after construction to open the audio device
    void initialise();

    // Playback control
    void play();
    void pause();
    void stop();
    void setCurrentPosition(double positionSeconds);
    void loadTrack(const juce::File& file);
    void loadXtpSequence(const juce::File& file);
    void loadStream(const juce::String& url, bool reconnecting = false, int reconnectAttempt = 0);
    bool isPlaying() const;

    // Playback quality
    // Requests a new output sample rate. Returns false if the device has no such
    // rate or rejected the change (in which case the previous rate is restored).
    bool setSampleRate(double sampleRate);
    void setBitDepth(int bitDepth);
    void setStereoExpansionMode(StereoExpansionMode mode) { stereoExpansionMode.store(mode); }
    StereoExpansionMode getStereoExpansionMode() const { return stereoExpansionMode.load(); }
    void setSpeakerTestEnabled(bool enabled) { speakerTestEnabled.store(enabled); }
    bool isSpeakerTestEnabled() const { return speakerTestEnabled.load(); }
    void advanceSpeakerTestChannel();
    int getSpeakerTestChannel() const { return speakerTestChannel.load(); }
    double getCurrentSampleRate() const { return currentSampleRate; }
    int getCurrentBitDepth() const { return currentBitDepth.load(); }
    int getCurrentProgramChannels() const { return activeProgramChannels.load(); }
    int getActiveOutputChannels() const { return activeOutputChannels.load(); }

    // Sample rates advertised by the current output device (empty if none open).
    juce::Array<double> getSupportedSampleRates() const;

    // Quantizes one sample to the given output bit depth, adding ditherLSB (in LSB
    // units) before rounding. Returns the input unchanged at >= 32 bits. Static and
    // pure so it can be unit-tested deterministically (pass ditherLSB = 0).
    static float quantizeSample(float sample, int bits, float ditherLSB);
    static juce::String describeChannelLayout(int channelCount);
    static void expandStereoToOutputChannels(juce::AudioBuffer<float>& buffer,
                                             int numOutputChannels,
                                             int numSamples,
                                             const juce::StringArray* outputChannelNames = nullptr);

    // Get audio for visualization.
    // Call tryLockAudioBuffer() first; if it returns true, call unlockAudioBuffer() when done.
    bool tryLockAudioBuffer()  { return audioBufferLock.tryEnter(); }
    void unlockAudioBuffer()   { audioBufferLock.exit(); }
    const juce::AudioBuffer<float>& getAudioBuffer() const { return audioBuffer; }
    double getCurrentPlaybackTime() const;
    double getTotalDuration() const;

    // Effects
    Equalizer& getEqualizer() { return equalizer; }
    PluginChain& getPluginChain() { return pluginChain; }

    // Master volume (0.0 = silent, 1.0 = full)
    void  setMasterGain(float gain) { masterGain.store(juce::jlimit(0.0f, 1.0f, gain)); }
    float getMasterGain() const     { return masterGain.load(); }

    // Recording
    void startRecording(const juce::File& outputFile);
    void stopRecording();
    bool isRecording() const { return activeWriter.load() != nullptr; }
    double getRecordingDuration() const;
    bool isCurrentlyStreaming() const { return currentlyStreaming.load(); }
    bool isStreamReconnectInProgress() const { return streamReconnectInProgress.load(); }
    bool didLastStreamReconnectFail() const { return streamReconnectFailed.load(); }

    // Module pattern data (populated when a module file is loaded)
    const ModulePatternData& getModulePatternData() const { return modulePatternData; }
    bool lastLoadedWasModule() const { return lastFileWasModule; }

    // Live playback position within the module (returns -1 if not applicable)
    int getCurrentPatternRow()   const;
    int getCurrentPatternIndex() const;

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // ChangeListener (responds to PlaylistManager changes)
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

private:
    // Applies currentSampleRate to the device. Returns false on failure and syncs
    // currentSampleRate to the rate the device actually adopted on success.
    bool updateAudioDevice();

    PlaylistManager* playlistManager;

    std::unique_ptr<juce::AudioFormatManager> formatManager;
    std::unique_ptr<MultiChannelAudioFormatReaderSource> readerSource;
    std::unique_ptr<XtpSequencerSource> xtpSource;
    std::unique_ptr<juce::AudioTransportSource> transportSource;
    std::unique_ptr<juce::AudioDeviceManager> deviceManager;
    std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;

    std::atomic<int> activeOutputChannels { 2 };
    std::atomic<int> activeProgramChannels { 2 };
    juce::StringArray activeOutputChannelNames;

    // Buffer for visualization (written on audio thread, read on GUI thread)
    juce::AudioBuffer<float> audioBuffer;
    juce::CriticalSection audioBufferLock;

    double currentSampleRate = 44100.0;
    std::atomic<int> currentBitDepth { 16 }; // output requantization depth (32 = float passthrough)
    std::atomic<StereoExpansionMode> stereoExpansionMode { StereoExpansionMode::Native };
    std::atomic<bool> speakerTestEnabled { false };
    std::atomic<int> speakerTestChannel { 0 };
    double speakerTestPhase = 0.0;
    int lastLoadedIndex = -1;

    // Requantizes the output buffer to currentBitDepth with TPDF dither.
    // No-op at 32-bit (full float). Called only on the audio thread.
    void applyBitDepth(juce::AudioBuffer<float>& buffer, int numChannels, int numSamples);
    juce::Random ditherRandom; // audio-thread only

    // Effects chain
    Equalizer  equalizer;
    PluginChain pluginChain;

    std::atomic<float> masterGain { 1.0f };

    // Background stream loader (stoppable, unlike juce::Thread::launch)
    struct StreamLoader : public juce::Thread
    {
        StreamLoader() : juce::Thread("stream-loader") {}
        std::function<void()> task;
        void run() override { if (task) task(); }
    };
    std::unique_ptr<StreamLoader> streamLoader;

    // Streaming read-ahead
    juce::TimeSliceThread readAheadThread { "stream-readahead" };
    std::atomic<bool> currentlyStreaming { false };
    std::atomic<bool> streamReconnectInProgress { false };
    std::atomic<bool> streamReconnectFailed { false };
    juce::String currentStreamUrl;
    bool streamWasPaused = false;
    juce::int64 streamPauseStartedMs = 0;
    // Shared so callAsync lambdas can safely check it after AudioEngine is destroyed.
    std::shared_ptr<std::atomic<int>> streamLoadGeneration { std::make_shared<std::atomic<int>>(0) };
    // Old loaders whose network call hasn't timed out yet. Kept alive so ~Thread
    // doesn't assert, then reaped once they finish. Cleaned up at next loadStream()
    // call and at destruction.
    std::vector<std::unique_ptr<StreamLoader>> zombieLoaders;

    // Recording
    juce::TimeSliceThread                               recordingThread { "recording" };
    juce::WavAudioFormat                                wavFormat;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*>    activeWriter { nullptr };
    std::atomic<juce::int64>                            recordingSampleCount { 0 };

    // Module pattern data (valid when last loaded file is a module)
    ModulePatternData modulePatternData;
    bool lastFileWasModule = false;

    // Non-owning pointer to the current openmpt reader (owned by readerSource)
    // Valid only while readerSource is alive and lastFileWasModule is true
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    OpenMptAudioFormatReader* openMptReader = nullptr;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
