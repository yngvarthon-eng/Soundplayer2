#include "AudioEngine.h"
#include "ModPlugFormat.h"
#include "../Playlist/PlaylistManager.h"
#include <cmath>

namespace
{
    constexpr int kRequestedMaxOutputChannels = 8;
    constexpr juce::int64 kStreamReconnectAfterPauseMs = 15000;
    constexpr int kMaxReconnectRetries = 1;
    constexpr int kStreamReconnectRetryDelayMs = 1500;

    juce::String joinChannelNames(const juce::StringArray& names)
    {
        return names.joinIntoString(", ");
    }

    juce::StringArray getCompactActiveOutputChannelNames(juce::AudioIODevice* device)
    {
        juce::StringArray compactNames;

        if (device == nullptr)
            return compactNames;

        const auto allNames = device->getOutputChannelNames();
        const auto activeBits = device->getActiveOutputChannels();

        for (int index = 0; index < allNames.size(); ++index)
            if (activeBits[index])
                compactNames.add(allNames[index]);

        if (compactNames.isEmpty())
            compactNames = allNames;

        return compactNames;
    }
}

std::unique_ptr<juce::AudioFormatManager> AudioEngine::createStandaloneFormatManager()
{
    auto fm = std::make_unique<juce::AudioFormatManager>();
    fm->registerBasicFormats();
#if defined(SOUNDPLAYER_OPENMPT_SUPPORT) || defined(SOUNDPLAYER_XMP_SUPPORT)
    fm->registerFormat(new OpenMptAudioFormat(), false);
#endif
    return fm;
}

AudioEngine::AudioEngine(PlaylistManager* playlistMgr)
    : playlistManager(playlistMgr)
{
    formatManager = createStandaloneFormatManager();

    deviceManager = std::make_unique<juce::AudioDeviceManager>();

    transportSource = std::make_unique<juce::AudioTransportSource>();
    resamplingSource = std::make_unique<juce::ResamplingAudioSource>(
        transportSource.get(), false, kRequestedMaxOutputChannels);

    audioBuffer.setSize(2, 512);
    audioBuffer.clear();

    playlistManager->addChangeListener(this);
    // Note: call initialise() after construction to open the audio device
}

void AudioEngine::initialise()
{
    readAheadThread.startThread(juce::Thread::Priority::normal);

    juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
    preferredSetup.useDefaultInputChannels = true;
    preferredSetup.useDefaultOutputChannels = true;
    preferredSetup.bufferSize = 512;

    auto error = deviceManager->initialise(0, kRequestedMaxOutputChannels, nullptr, true, {}, &preferredSetup);
    if (error.isNotEmpty())
    {
        // No audio device found — app still works, just no sound
        return;
    }

    if (auto* device = deviceManager->getCurrentAudioDevice())
    {
        activeOutputChannels.store(juce::jmax(2, device->getActiveOutputChannels().countNumberOfSetBits()));
        activeOutputChannelNames = getCompactActiveOutputChannelNames(device);
        fprintf(stderr, "AudioEngine: opened device '%s' outputs=%d [%s]\n",
                device->getName().toRawUTF8(),
                activeOutputChannels.load(),
                joinChannelNames(activeOutputChannelNames).toRawUTF8());
    }

    deviceManager->addAudioCallback(this);
}

AudioEngine::~AudioEngine()
{
    // Invalidate all pending callAsync lambdas *before* anything is torn down so
    // that lambdas queued but not yet executed on the message thread bail out safely
    // without touching any member variables.
    ++(*streamLoadGeneration);

    // Signal every loader thread to exit, then wait.  We do the waiting here (in
    // the destructor) rather than in loadStream() so we never block the message
    // thread while the network I/O times out.
    if (streamLoader)
        streamLoader->signalThreadShouldExit();
    for (auto& z : zombieLoaders)
        z->signalThreadShouldExit();

    if (streamLoader && streamLoader->isThreadRunning())
        streamLoader->stopThread(12000);
    streamLoader.reset();

    for (auto& z : zombieLoaders)
        z->stopThread(12000);
    zombieLoaders.clear();

    // Stop recording first (flushes writer)
    stopRecording();

    deviceManager->removeAudioCallback(this);
    if (playlistManager != nullptr)
        playlistManager->removeChangeListener(this);

    transportSource->stop();
    transportSource->setSource(nullptr);
    readerSource.reset();
    xtpSource.reset();

    deviceManager->closeAudioDevice();
    readAheadThread.stopThread(2000);
}

void AudioEngine::play()
{
    if (readerSource == nullptr)
    {
        // Nothing loaded — select first track if available
        if (playlistManager->getItems().size() > 0 && playlistManager->getCurrentIndex() < 0)
            playlistManager->selectItem(0);
        return;
    }

    if (currentlyStreaming.load() && streamWasPaused)
    {
        const auto pausedMs = juce::Time::currentTimeMillis() - streamPauseStartedMs;
        streamWasPaused = false;

        // Many HTTP streams drop idle clients while paused; reconnect instead of
        // resuming a stale decoder/connection after a long pause.
        if (pausedMs >= kStreamReconnectAfterPauseMs && currentStreamUrl.isNotEmpty())
        {
            loadStream(currentStreamUrl, true, 0);
            return;
        }
    }

    transportSource->start();
}

void AudioEngine::pause()
{
    if (currentlyStreaming.load())
    {
        streamWasPaused = true;
        streamPauseStartedMs = juce::Time::currentTimeMillis();
    }

    transportSource->stop();
}

void AudioEngine::stop()
{
    streamWasPaused = false;
    transportSource->stop();
    if (readerSource != nullptr && !currentlyStreaming.load())
        transportSource->setPosition(0.0);
}

void AudioEngine::setCurrentPosition(double positionSeconds)
{
    transportSource->setPosition(positionSeconds);
}

void AudioEngine::loadTrack(const juce::File& file)
{
    ++(*streamLoadGeneration); // cancel any in-flight stream load
    transportSource->stop();
    transportSource->setSource(nullptr);
    transportSource->setPosition(0.0);  // reset so new track always starts from zero
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    openMptReader = nullptr; // clear before readerSource.reset() deletes the reader
#endif
    readerSource.reset();
    xtpSource.reset();

    modulePatternData.clear();
    currentStreamUrl.clear();
    streamWasPaused = false;
    activeProgramChannels.store(2);
    currentlyStreaming.store(false);
    streamReconnectInProgress.store(false);
    streamReconnectFailed.store(false);
    auto ext = file.getFileExtension().toLowerCase();
    lastFileWasModule = (ext == ".mod" || ext == ".xm" || ext == ".it" || ext == ".s3m"
                         || ext == ".mptm" || ext == ".umx" || ext == ".ahx"
                         || ext == ".ams" || ext == ".amf" || ext == ".med"
                         || ext == ".mo3" || ext == ".mtm" || ext == ".okt"
                         || ext == ".oct" || ext == ".stm" || ext == ".669"
                         || ext == ".umf" || ext == ".drm" || ext == ".digi"
                         || ext == ".far" || ext == ".mdl" || ext == ".mt2"
                         || ext == ".dcm" || ext == ".dlm" || ext == ".ptm"
                         || ext == ".pcm" || ext == ".dbm" || ext == ".dsm"
                         || ext == ".cdm" || ext == ".hvl" || ext == ".c67"
                         || ext == ".sfx" || ext == ".imf" || ext == ".u2b"
                         || ext == ".psm" || ext == ".stp" || ext == ".symmod");
    if (lastFileWasModule)
        modulePatternData.loadFromFile(file);

    auto* reader = formatManager->createReaderFor(file);
    if (reader != nullptr)
    {
        activeProgramChannels.store(juce::jlimit(1, kRequestedMaxOutputChannels, (int) reader->numChannels));
        fprintf(stderr, "AudioEngine: loaded '%s' sampleRate=%.0f channels=%d length=%.1fs\n",
                file.getFileName().toRawUTF8(), reader->sampleRate,
                (int)reader->numChannels, reader->lengthInSamples / reader->sampleRate);
        readerSource = std::make_unique<MultiChannelAudioFormatReaderSource>(reader, true);
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
        if (lastFileWasModule)
            openMptReader = dynamic_cast<OpenMptAudioFormatReader*>(reader);
#endif
        transportSource->setSource(readerSource.get(), 0, nullptr, reader->sampleRate,
                       activeProgramChannels.load());
        transportSource->start();
    }
}

void AudioEngine::loadXtpSequence(const juce::File& file)
{
    ++(*streamLoadGeneration); // cancel any in-flight stream load
    transportSource->stop();
    transportSource->setSource(nullptr);
    transportSource->setPosition(0.0);
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    openMptReader = nullptr;
#endif
    readerSource.reset();
    xtpSource.reset();

    modulePatternData.clear();
    currentStreamUrl.clear();
    streamWasPaused = false;
    activeProgramChannels.store(2);
    currentlyStreaming.store(false);
    streamReconnectInProgress.store(false);
    streamReconnectFailed.store(false);
    lastFileWasModule = false;

    auto newSource = std::make_unique<XtpSequencerSource>(file, currentSampleRate);
    if (!newSource->isValid())
    {
        fprintf(stderr, "AudioEngine: failed to load '%s': %s\n",
                file.getFileName().toRawUTF8(), newSource->getLoadError().toRawUTF8());
        return;
    }

    // Reuses the module pattern viewer: ModuleNote's fields are plain display
    // strings, not libopenmpt-specific, so .xtp patterns render through the same UI.
    modulePatternData.loadFromXtpSong(newSource->getSong());
    lastFileWasModule = true;

    xtpSource = std::move(newSource);
    fprintf(stderr, "AudioEngine: loaded xtp sequence '%s' patterns=%d channels=%d\n",
            file.getFileName().toRawUTF8(), (int) xtpSource->getSong().patterns.size(), xtpSource->getSong().channels);

    transportSource->setSource(xtpSource.get(), 0, nullptr, XtpSequencerSource::kRenderSampleRate,
                               activeProgramChannels.load());
    transportSource->start();
}

void AudioEngine::loadStream(const juce::String& url, bool reconnecting, int reconnectAttempt)
{
    // Tear down current playback on the message thread (safe to do here).
    transportSource->stop();
    transportSource->setSource(nullptr);
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    openMptReader = nullptr;
#endif
    readerSource.reset();
    xtpSource.reset();
    modulePatternData.clear();
    lastFileWasModule = false;
    activeProgramChannels.store(2);
    currentlyStreaming.store(false);
    streamReconnectInProgress.store(reconnecting);
    streamReconnectFailed.store(false);
    currentStreamUrl = url;
    streamWasPaused = false;

    // Bump the generation so any in-flight load from a previous call is discarded.
    int generation = ++(*streamLoadGeneration);

    // Signal the previous loader to exit but don't wait — its network I/O may take
    // several seconds to time out, and we're on the message thread.  Park it in the
    // zombie list so its destructor isn't called while it's still running; reap any
    // zombies that have already finished while we're here.
    if (streamLoader)
        streamLoader->signalThreadShouldExit();
    zombieLoaders.erase(
        std::remove_if(zombieLoaders.begin(), zombieLoaders.end(),
                       [](const auto& t) { return !t->isThreadRunning(); }),
        zombieLoaders.end());
    if (streamLoader && streamLoader->isThreadRunning())
        zombieLoaders.push_back(std::move(streamLoader));
    else
        streamLoader.reset();

    streamLoader = std::make_unique<StreamLoader>();

    // createInputStream() is a blocking network call — run it off the message thread
    // to keep the UI responsive. Once the reader is ready, marshal back to the
    // message thread for the JUCE audio-graph operations (setSource/start).
    // genToken is a shared_ptr copy so the generation check in the callAsync lambda
    // remains safe even if AudioEngine is destroyed before the lambda fires.
    auto genToken = streamLoadGeneration;
    streamLoader->task = [this, url, generation, genToken, reconnecting, reconnectAttempt]()
    {
        auto markReconnectFailedIfCurrent = [this, generation, genToken]()
        {
            if (genToken->load() == generation)
            {
                streamReconnectInProgress.store(false);
                streamReconnectFailed.store(true);
            }
        };

        auto scheduleReconnectRetryIfCurrent = [this, url, generation, genToken, reconnectAttempt]()
        {
            juce::Timer::callAfterDelay(kStreamReconnectRetryDelayMs, [this, url, generation, genToken, reconnectAttempt]()
            {
                if (genToken->load() != generation)
                    return;

                loadStream(url, true, reconnectAttempt + 1);
            });
        };

        // Resolve PLS / M3U playlist files to the underlying stream URL before
        // attempting to open an audio format reader.
        auto resolvePlaylist = [](const juce::String& rawUrl) -> juce::String
        {
            juce::String lower = rawUrl.toLowerCase();
            if (!lower.endsWith(".pls") && !lower.endsWith(".m3u") && !lower.endsWith(".m3u8"))
                return rawUrl;

            juce::URL playlistUrl(rawUrl);
            auto stream = playlistUrl.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(10000)
                    .withNumRedirectsToFollow(5));
            if (stream == nullptr)
                return rawUrl;

            juce::String content = stream->readEntireStreamAsString();

            if (lower.endsWith(".pls"))
            {
                // PLS format: look for "File1=<url>" (case-insensitive key)
                for (auto& line : juce::StringArray::fromLines(content))
                {
                    if (line.startsWithIgnoreCase("File") && line.contains("="))
                    {
                        juce::String val = line.fromFirstOccurrenceOf("=", false, false).trim();
                        if (val.startsWithIgnoreCase("http"))
                            return val;
                    }
                }
            }
            else
            {
                // M3U format: first non-comment, non-empty line is the URL
                for (auto& line : juce::StringArray::fromLines(content))
                {
                    juce::String trimmed = line.trim();
                    if (trimmed.isNotEmpty() && !trimmed.startsWith("#"))
                        return trimmed;
                }
            }

            return rawUrl; // couldn't parse, try original
        };

        juce::String resolvedUrl = resolvePlaylist(url);
        if (resolvedUrl != url)
            fprintf(stderr, "AudioEngine: playlist resolved to '%s'\n", resolvedUrl.toRawUTF8());

        juce::URL juceUrl(resolvedUrl);
        auto inputStream = juceUrl.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
                .withNumRedirectsToFollow(5));

        if (inputStream == nullptr)
        {
            fprintf(stderr, "AudioEngine: failed to open stream '%s'\n", resolvedUrl.toRawUTF8());
            if (reconnecting && reconnectAttempt < kMaxReconnectRetries)
            {
                scheduleReconnectRetryIfCurrent();
            }
            else
            {
                markReconnectFailedIfCurrent();
            }
            return;
        }

        if (genToken->load() != generation)
            return; // A newer load was requested; discard this connection.

        // HTTP streams aren't seekable. AudioFormatManager rewinds the stream between
        // format-probing attempts, which triggers a jassert. Wrapping in
        // BufferedInputStream lets JUCE seek within its in-memory cache.
        auto bufferedStream = std::make_unique<juce::BufferedInputStream>(
            inputStream.release(), 65536, true);

        auto* reader = formatManager->createReaderFor(std::move(bufferedStream));
        if (reader == nullptr)
        {
            fprintf(stderr, "AudioEngine: no format reader for stream '%s'\n", resolvedUrl.toRawUTF8());
            if (reconnecting && reconnectAttempt < kMaxReconnectRetries)
            {
                scheduleReconnectRetryIfCurrent();
            }
            else
            {
                markReconnectFailedIfCurrent();
            }
            return;
        }

        fprintf(stderr, "AudioEngine: streaming '%s' sampleRate=%.0f channels=%d\n",
                resolvedUrl.toRawUTF8(), reader->sampleRate, (int)reader->numChannels);

        double sr = reader->sampleRate;

        // A malformed/partial stream can hand back a reader with a nonsensical
        // format. Feeding a zero or negative rate into JUCE's resampler would
        // trip an assertion (fatal in debug builds) instead of failing cleanly.
        if (sr <= 0.0 || reader->numChannels == 0)
        {
            fprintf(stderr, "AudioEngine: stream '%s' reported an invalid format "
                             "(sampleRate=%.0f channels=%d) — aborting\n",
                    resolvedUrl.toRawUTF8(), sr, (int) reader->numChannels);
            delete reader;
            if (reconnecting && reconnectAttempt < kMaxReconnectRetries)
                scheduleReconnectRetryIfCurrent();
            else
                markReconnectFailedIfCurrent();
            return;
        }

        // Wrap reader in a shared_ptr<unique_ptr<>> so it is safely deleted
        // if the callAsync lambda is discarded before the message thread fires it.
        auto ownedReader = std::make_shared<std::unique_ptr<juce::AudioFormatReader>>(reader);

        juce::MessageManager::callAsync([this, generation, genToken, ownedReader, sr]()
        {
            // genToken is a shared_ptr — safe to dereference even if AudioEngine is gone.
            // If the generation no longer matches, AudioEngine was destroyed or a newer
            // load was requested; either way, don't touch *this*.
            if (genToken->load() != generation)
                return; // ownedReader destroyed here → reader deleted

            // Radio streams are almost always 44.1kHz/16-bit. Rather than relying on
            // JUCE's on-the-fly resampler to reconcile the stream's rate against
            // whatever the output device is currently set to (which is fragile if the
            // device was left on a rate meant for local high-res playback), align the
            // device to the stream's own rate first — setSampleRate() validates
            // against the hardware and safely reverts if unsupported.
            setSampleRate(sr);

            activeProgramChannels.store(juce::jlimit(1, kRequestedMaxOutputChannels,
                                                     (int) (*ownedReader)->numChannels));
            readerSource = std::make_unique<MultiChannelAudioFormatReaderSource>(
                ownedReader->release(), true);
            // Use read-ahead buffering (~1.5 s at 44100 Hz) to smooth over network jitter.
            transportSource->setSource(readerSource.get(), 65536, &readAheadThread, sr,
                                       activeProgramChannels.load());
            transportSource->start();
            currentlyStreaming.store(true);
            streamReconnectInProgress.store(false);
            streamReconnectFailed.store(false);
        });
    };
    streamLoader->startThread();
}

bool AudioEngine::isPlaying() const
{
    if (streamWasPaused)
        return false;

    return transportSource->isPlaying();
}

double AudioEngine::getCurrentPlaybackTime() const
{
    return transportSource->getCurrentPosition();
}

double AudioEngine::getTotalDuration() const
{
    return transportSource->getLengthInSeconds();
}

juce::Array<double> AudioEngine::getSupportedSampleRates() const
{
    if (auto* device = deviceManager->getCurrentAudioDevice())
        return device->getAvailableSampleRates();
    return {};
}

bool AudioEngine::setSampleRate(double sampleRate)
{
    if (currentSampleRate == sampleRate)
        return true;

    auto* device = deviceManager->getCurrentAudioDevice();
    if (device == nullptr)
        return false;

    // Never force a rate the hardware doesn't advertise — doing so can feed a
    // DAC/amplifier a stream it can't lock onto, tripping its protection mute.
    bool supported = false;
    for (auto rate : device->getAvailableSampleRates())
        if (std::abs(rate - sampleRate) < 1.0)
        {
            supported = true;
            break;
        }

    if (!supported)
    {
        fprintf(stderr, "AudioEngine: device does not support %.0f Hz — ignoring\n", sampleRate);
        return false;
    }

    const double previousRate = currentSampleRate;
    currentSampleRate = sampleRate;

    if (!updateAudioDevice())
    {
        // Restore the last known-good rate so the device keeps working.
        currentSampleRate = previousRate;
        updateAudioDevice();
        return false;
    }

    return true;
}

void AudioEngine::advanceSpeakerTestChannel()
{
    const int channelCount = juce::jmax(1, activeOutputChannels.load());
    speakerTestChannel.store((speakerTestChannel.load() + 1) % channelCount);
}

void AudioEngine::setBitDepth(int bitDepth)
{
    currentBitDepth.store(bitDepth);
}

float AudioEngine::quantizeSample(float sample, int bits, float ditherLSB)
{
    if (bits >= 32)
        return sample; // full 32-bit float path — nothing to quantize

    // step = 1 / 2^(bits-1) is one LSB at full scale (16-bit -> 32768 levels).
    const float step = 1.0f / (float) (1 << (bits - 1));
    const float quantized = std::round(sample / step + ditherLSB) * step;
    return juce::jlimit(-1.0f, 1.0f, quantized);
}

juce::String AudioEngine::describeChannelLayout(int channelCount)
{
    switch (channelCount)
    {
        case 1: return "Mono";
        case 2: return "Stereo";
        case 3: return "3.0";
        case 4: return "Quad";
        case 5: return "5.0";
        case 6: return "5.1";
        case 7: return "6.1";
        case 8: return "7.1";
        default: break;
    }

    return juce::String(channelCount) + " ch";
}

void AudioEngine::expandStereoToOutputChannels(juce::AudioBuffer<float>& buffer,
                                               int numOutputChannels,
                                               int numSamples,
                                               const juce::StringArray* outputChannelNames)
{
    if (numOutputChannels <= 2 || buffer.getNumChannels() < 2)
        return;

    const auto* left = buffer.getReadPointer(0);
    const auto* right = buffer.getReadPointer(1);
    juce::HeapBlock<float> mono(numSamples);
    for (int i = 0; i < numSamples; ++i)
        mono[i] = 0.5f * (left[i] + right[i]);

    auto getRoleForChannel = [numOutputChannels, outputChannelNames](int ch)
    {
        if (outputChannelNames != nullptr && ch < outputChannelNames->size())
        {
            auto name = outputChannelNames->operator[](ch).toLowerCase();

            // Backends like PipeWire/ALSA may expose only generic labels such as
            // "channel 1", which carry no speaker-role information. In that case,
            // do not guess center/LFE placement or we may mute the actual rear pair.
            if (name.startsWith("channel "))
                return (ch % 2 == 0) ? juce::String("left") : juce::String("right");

            if (name.contains("lfe") || name.contains("sub") || name.contains("woofer"))
                return juce::String("lfe");
            if (name.contains("centre") || name.contains("center") || name.contains("mono"))
                return juce::String("center");
            if ((name.contains("rear") || name.contains("side") || name.contains("surround") || name.contains("back"))
                && name.contains("left"))
                return juce::String("left");
            if ((name.contains("rear") || name.contains("side") || name.contains("surround") || name.contains("back"))
                && name.contains("right"))
                return juce::String("right");
            if (name.contains("left"))
                return juce::String("left");
            if (name.contains("right"))
                return juce::String("right");
        }

        // Conservative fallback for common compact 5.1/7.1 orders.
        if (numOutputChannels >= 6)
        {
            if (ch == 2)
                return juce::String("center");
            if (ch == 3)
                return juce::String("lfe");
            return (ch % 2 == 0) ? juce::String("left") : juce::String("right");
        }

        return (ch % 2 == 0) ? juce::String("left") : juce::String("right");
    };

    for (int ch = 2; ch < numOutputChannels; ++ch)
    {
        const auto role = getRoleForChannel(ch);

        if (role == "lfe")
        {
            buffer.clear(ch, 0, numSamples);
            continue;
        }

        if (role == "center")
        {
            buffer.copyFrom(ch, 0, mono.getData(), numSamples);
            continue;
        }

        buffer.copyFrom(ch, 0, role == "right" ? right : left, numSamples);
    }
}

void AudioEngine::applyBitDepth(juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
{
    const int bits = currentBitDepth.load();
    if (bits >= 32)
        return; // full 32-bit float path — nothing to quantize

    // Add triangular (TPDF) dither of ±1 LSB so quantization noise is decorrelated
    // from the signal instead of producing harmonic distortion.
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float dither = ditherRandom.nextFloat() - ditherRandom.nextFloat(); // (-1, 1) LSB
            data[i] = quantizeSample(data[i], bits, dither);
        }
    }
}

bool AudioEngine::updateAudioDevice()
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager->getAudioDeviceSetup(setup);
    setup.sampleRate = currentSampleRate;
    setup.bufferSize = 512;

    if (auto* device = deviceManager->getCurrentAudioDevice())
    {
        if (auto defaultOutputs = device->getDefaultOutputChannels())
        {
            setup.outputChannels = *defaultOutputs;
            setup.useDefaultOutputChannels = false;
        }
        else
        {
            const auto activeOutputs = device->getActiveOutputChannels();
            if (activeOutputs.countNumberOfSetBits() > 0)
            {
                setup.outputChannels = activeOutputs;
                setup.useDefaultOutputChannels = false;
            }
            else
            {
                setup.useDefaultOutputChannels = true;
            }
        }
    }

    auto error = deviceManager->setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
    {
        fprintf(stderr, "AudioEngine: failed to set %.0f Hz: %s\n",
                currentSampleRate, error.toRawUTF8());
        return false;
    }

    // The device may have adopted a different rate than requested; reflect reality
    // so the UI, recording, and timing all use the true rate.
    if (auto* device = deviceManager->getCurrentAudioDevice())
    {
        currentSampleRate = device->getCurrentSampleRate();
        activeOutputChannels.store(juce::jmax(2, device->getActiveOutputChannels().countNumberOfSetBits()));
        activeOutputChannelNames = getCompactActiveOutputChannelNames(device);
        fprintf(stderr, "AudioEngine: active outputs=%d [%s]\n",
                activeOutputChannels.load(),
                joinChannelNames(activeOutputChannelNames).toRawUTF8());
    }

    return true;
}

// ---- Recording ----

void AudioEngine::startRecording(const juce::File& outputFile)
{
    if (isRecording())
        stopRecording();

    recordingThread.startThread(juce::Thread::Priority::normal);

    auto outputStream = std::make_unique<juce::FileOutputStream>(outputFile);
    if (outputStream->failedToOpen())
    {
        fprintf(stderr, "AudioEngine: failed to open recording file '%s'\n",
                outputFile.getFullPathName().toRawUTF8());
        recordingThread.stopThread(500);
        return;
    }

    const int numChannels = juce::jmax(2, activeOutputChannels.load());

    // 16-bit WAV matching the active output channel count.
    auto* wavWriter = wavFormat.createWriterFor(
        outputStream.get(),
        currentSampleRate,
        numChannels,
        16,
        {},
        0);

    if (wavWriter == nullptr)
    {
        recordingThread.stopThread(500);
        return;
    }

    outputStream.release(); // writer now owns the stream
    writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        wavWriter, recordingThread, 65536);
    recordingSampleCount.store(0);
    activeWriter.store(writer.get());
}

void AudioEngine::stopRecording()
{
    activeWriter.store(nullptr);      // audio thread stops writing immediately
    writer.reset();                   // flushes FIFO and closes file
    recordingThread.stopThread(2000);
}

double AudioEngine::getRecordingDuration() const
{
    if (currentSampleRate <= 0.0)
        return 0.0;
    return static_cast<double>(recordingSampleCount.load()) / currentSampleRate;
}

// ---- AudioIODeviceCallback ----

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    speakerTestChannel.store(0);
    speakerTestPhase = 0.0;

    if (device == nullptr)
        return; // No device to prepare against — avoid dereferencing a null pointer below.

    activeOutputChannels.store(juce::jmax(2, device->getActiveOutputChannels().countNumberOfSetBits()));
    activeOutputChannelNames = getCompactActiveOutputChannelNames(device);
    fprintf(stderr, "AudioEngine: callback device '%s' outputs=%d [%s]\n",
            device->getName().toRawUTF8(),
            activeOutputChannels.load(),
            joinChannelNames(activeOutputChannelNames).toRawUTF8());

    double sampleRate = device->getCurrentSampleRate();
    int bufferSize = device->getCurrentBufferSizeSamples();

    resamplingSource->prepareToPlay(bufferSize, sampleRate);
    equalizer.prepare(sampleRate, bufferSize, activeOutputChannels.load());
    pluginChain.prepare(sampleRate, bufferSize);
}

void AudioEngine::audioDeviceStopped()
{
    resamplingSource->releaseResources();
}

void AudioEngine::audioDeviceIOCallbackWithContext(
    const float* const* /*inputChannelData*/,
    int /*numInputChannels*/,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    // Wrap the device output buffers in a JUCE AudioBuffer
    juce::AudioBuffer<float> outputBuffer(outputChannelData, numOutputChannels, numSamples);
    outputBuffer.clear();

    if (speakerTestEnabled.load())
    {
        const int testChannel = juce::jlimit(0, juce::jmax(0, numOutputChannels - 1), speakerTestChannel.load());
        const double sampleRate = juce::jmax(1.0, currentSampleRate);
        const double phaseDelta = juce::MathConstants<double>::twoPi * 440.0 / sampleRate;
        float* dest = outputBuffer.getWritePointer(testChannel);

        for (int i = 0; i < numSamples; ++i)
        {
            dest[i] = 0.2f * std::sin(speakerTestPhase);
            speakerTestPhase += phaseDelta;
            if (speakerTestPhase >= juce::MathConstants<double>::twoPi)
                speakerTestPhase -= juce::MathConstants<double>::twoPi;
        }

        applyBitDepth(outputBuffer, numOutputChannels, numSamples);
        return;
    }

    const int programChannels = juce::jlimit(1, juce::jmax(1, numOutputChannels), activeProgramChannels.load());

    juce::AudioSourceChannelInfo info(outputBuffer);
    resamplingSource->getNextAudioBlock(info);

    // Apply equalizer, plugin chain, and master gain
    equalizer.process(outputBuffer, juce::jmin(programChannels, numOutputChannels), numSamples);
    pluginChain.process(outputBuffer);

    if (programChannels == 2
        && stereoExpansionMode.load() == StereoExpansionMode::MultiSpeakerStereo)
    {
        expandStereoToOutputChannels(outputBuffer, numOutputChannels, numSamples,
                                     &activeOutputChannelNames);
    }

    outputBuffer.applyGain(masterGain.load());

    // Requantize to the selected output bit depth (dithered). Done last so the
    // visualization and recording reflect exactly what is sent to the device.
    applyBitDepth(outputBuffer, numOutputChannels, numSamples);

    // Copy to visualization buffer (best-effort, no blocking)
    if (audioBufferLock.tryEnter())
    {
        audioBuffer.setSize(programChannels, numSamples, false, false, true);
        for (int ch = 0; ch < programChannels; ++ch)
            audioBuffer.copyFrom(ch, 0, outputBuffer, ch, 0, numSamples);
        audioBufferLock.exit();
    }

    // Record processed output (lockless — atomic pointer)
    if (auto* w = activeWriter.load())
    {
        const float* recordingChannels[kRequestedMaxOutputChannels] = {};
        for (int ch = 0; ch < programChannels; ++ch)
            recordingChannels[ch] = outputBuffer.getReadPointer(ch);

        w->write(recordingChannels, numSamples);
        recordingSampleCount.fetch_add(numSamples);
    }
}

// ---- Module playback position ----

int AudioEngine::getCurrentPatternRow() const
{
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    if (openMptReader != nullptr)
        return openMptReader->getCurrentRow();
#endif
    if (xtpSource != nullptr)
        return xtpSource->getCurrentRow();
    return -1;
}

int AudioEngine::getCurrentPatternIndex() const
{
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    if (openMptReader != nullptr)
        return openMptReader->getCurrentPattern();
#endif
    if (xtpSource != nullptr)
        return xtpSource->getCurrentPatternIndex();
    return -1;
}

// ---- ChangeListener ----

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster*)
{
    int newIndex = playlistManager->getCurrentIndex();

    if (newIndex < 0)
    {
        if (lastLoadedIndex >= 0)
        {
            // Playlist was cleared
            transportSource->stop();
            transportSource->setSource(nullptr);
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
            openMptReader = nullptr;
#endif
            readerSource.reset();
            xtpSource.reset();
            lastLoadedIndex = -1;
        }
        return;
    }

    if (newIndex != lastLoadedIndex)
    {
        lastLoadedIndex = newIndex;
        auto* item = playlistManager->getCurrentItem();
        if (item != nullptr)
        {
            if (item->isStream)
                loadStream(item->streamUrl);
            else if (item->isXtpSequence)
                loadXtpSequence(item->file);
            else
                loadTrack(item->file);
        }
    }
}
