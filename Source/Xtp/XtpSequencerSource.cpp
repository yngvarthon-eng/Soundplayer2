#include "XtpSequencerSource.h"

#if defined(SOUNDPLAYER_FLUIDSYNTH_SUPPORT)
#include "XtpSf2Voice.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    // Equal-power center-pan gain -- matches the pan-law constant exTracker
    // itself uses (sqrt(0.5)). Every voice is summed to a single mono bus and
    // placed dead-center: real per-note panning (effect 0x08/E8x) has no
    // audible effect in exTracker either for any plugin-routed instrument
    // (which is the default/typical configuration -- see XtpSequencer.h for the
    // full explanation of why pitch/pan-continuous effects are inert there), so
    // there is nothing to reproduce by panning voices individually here.
    constexpr float kCenterGain = 0.70710678f;

    // Whether any pattern in the song uses a jump/loop effect (0x0B, 0x0D, or
    // E6x pattern-loop) whose actual traversal length can't be predicted by
    // summing row durations once through the song order -- used to pad the
    // reported total length generously so JUCE's EOF/position logic doesn't cut
    // playback off before the sequencer really finishes.
    bool songHasJumpOrLoopEffects(const xtp::Song& song)
    {
        for (const auto& pattern : song.patterns)
        {
            for (const auto& step : pattern.steps)
            {
                if (step.effectCommand == 0x0B || step.effectCommand == 0x0D)
                    return true;
                if (step.effectCommand == 0x0E && ((step.effectValue >> 4) & 0x0F) == 0x6)
                    return true;
            }
        }
        return false;
    }
}

XtpSequencerSource::XtpSequencerSource(const juce::File& file, double /*outputSampleRate*/)
{
    if (!song.loadFromFile(file, loadError))
        return;

    buildInstrumentVoices();

    double estimatedSeconds = song.estimateDurationSeconds();
    if (songHasJumpOrLoopEffects(song))
        estimatedSeconds *= 8.0; // generous headroom: see songHasJumpOrLoopEffects()
    totalLengthSamples = (juce::int64) (estimatedSeconds * sampleRate);

    seekToSampleZero();
}

XtpSequencerSource::~XtpSequencerSource() = default;

void XtpSequencerSource::buildInstrumentVoices()
{
    for (int slot = 0; slot < 16; ++slot)
    {
        const juce::String& id = song.instruments[(size_t) slot];
        if (id == "builtin.sine")
        {
            instrumentVoices[(size_t) slot] =
                std::make_unique<xtp::BuiltinOscillatorVoice>(xtp::BuiltinOscillatorVoice::Waveform::Sine);
        }
        else if (id == "builtin.square")
        {
            instrumentVoices[(size_t) slot] =
                std::make_unique<xtp::BuiltinOscillatorVoice>(xtp::BuiltinOscillatorVoice::Waveform::Square);
        }
        else if (id == "builtin.sample")
        {
            // No WAV path is attached to the instrument slot itself -- actual
            // sample audio only comes from the per-step `sample` bank-slot field,
            // which resolveVoice() already prefers over the instrument slot. An
            // instrument merely assigned "builtin.sample" with no per-note sample
            // reference has nothing to play.
        }
#if defined(SOUNDPLAYER_FLUIDSYNTH_SUPPORT)
        else if (id.startsWith("sf2:"))
        {
            // Treated as a direct filesystem path, not exTracker's own
            // pre-scanned-directory gating (an implementation quirk of
            // PluginHost, not a format requirement -- see XtpSequencer.h).
            const juce::File sf2File(id.substring(4));
            auto voice = std::make_unique<xtp::Sf2Voice>();
            if (voice->loadSoundFont(sf2File, sampleRate))
            {
                instrumentVoices[(size_t) slot] = std::move(voice);
            }
            else
            {
                fprintf(stderr, "XtpSequencerSource: instrument slot %d failed to load soundfont '%s' -- will be silent\n",
                        slot, sf2File.getFullPathName().toRawUTF8());
                instrumentVoices[(size_t) slot] = std::make_unique<xtp::SilentVoice>();
            }
        }
#endif
        else if (id.isNotEmpty())
        {
            fprintf(stderr,
                    "XtpSequencerSource: instrument slot %d requests unsupported backend '%s' -- will be silent\n",
                    slot, id.toRawUTF8());
            instrumentVoices[(size_t) slot] = std::make_unique<xtp::SilentVoice>();
        }
    }

    for (const auto& [slot, entry] : song.samples)
    {
        auto voice = std::make_unique<xtp::SampleVoice>();
        if (voice->loadSample(entry.path))
        {
            sampleVoices[slot] = std::move(voice);
        }
        else
        {
            fprintf(stderr, "XtpSequencerSource: failed to load sample slot %d ('%s') from '%s'\n",
                    slot, entry.name.toRawUTF8(), entry.path.getFullPathName().toRawUTF8());
        }
    }
}

xtp::InstrumentVoice* XtpSequencerSource::resolveVoice(std::uint8_t instrument, std::uint16_t sample)
{
    if (sample != 0xFFFF)
    {
        auto it = sampleVoices.find((int) sample);
        if (it != sampleVoices.end())
            return it->second.get();
    }

    if (instrument >= 16)
    {
        auto it = sampleVoices.find((int) instrument);
        return (it != sampleVoices.end()) ? it->second.get() : nullptr;
    }

    if (instrumentVoices[instrument])
        return instrumentVoices[instrument].get();

    auto it = sampleVoices.find((int) instrument);
    return (it != sampleVoices.end()) ? it->second.get() : nullptr;
}

void XtpSequencerSource::noteOn(std::uint8_t instrument, std::uint16_t sample, int midiNote,
                                std::uint8_t velocity, bool retrigger)
{
    if (suppressAudibleDispatch)
        return;
    if (auto* voice = resolveVoice(instrument, sample))
        voice->noteOn(midiNote, velocity, retrigger);
}

void XtpSequencerSource::noteOff(std::uint8_t instrument, std::uint16_t sample, int midiNote)
{
    if (suppressAudibleDispatch)
        return;
    if (auto* voice = resolveVoice(instrument, sample))
        voice->noteOff(midiNote);
}

void XtpSequencerSource::seekToSampleZero()
{
    sequencer.allNotesOff(*this);
    for (auto& v : instrumentVoices)
        if (v) v->allNotesOff();
    for (auto& [slot, v] : sampleVoices)
        if (v) v->allNotesOff();

    songOrderPosition = 0;
    currentPatternIndex = song.songOrder.empty()
        ? 0
        : juce::jlimit(0, juce::jmax(0, (int) song.patterns.size() - 1), song.songOrder[0]);

    transport.setTempoBpm(song.tempoBpm);
    transport.setTicksPerBeat(song.ticksPerBeat);
    transport.setTicksPerRow(song.ticksPerRow);
    transport.setPatternRows(song.patterns[(size_t) currentPatternIndex].rows);
    const int swing = (currentPatternIndex < (int) song.patternSwing.size())
        ? song.patternSwing[(size_t) currentPatternIndex] : 50;
    transport.setSwingPercent(swing);
    transport.reset(song.patterns[(size_t) currentPatternIndex].rows);

    sequencer.reset(song.channels);
    sequencer.setChannelVolumes(song.channelVolume);

    tickTimeAccumulator = 0.0;
    position = 0;
    finished = false;

    currentRowForUi.store(0);
    currentPatternForUi.store(currentPatternIndex);

    // Row 0 fires immediately at t=0, before any tick has elapsed -- matches
    // exTracker's own behavior (the sequencer dispatches whatever row the
    // transport is already on the first time it's polled).
    sequencer.onRowBoundary(song.patterns[(size_t) currentPatternIndex], transport, *this);
}

void XtpSequencerSource::advanceSongOrder()
{
    int nextPosition = songOrderPosition + 1;
    if (nextPosition >= (int) song.songOrder.size())
    {
        if (!looping)
        {
            finished = true;
            sequencer.allNotesOff(*this);
            return;
        }
        nextPosition = 0;
    }
    songOrderPosition = nextPosition;

    int nextPatternIndex = song.songOrder[(size_t) songOrderPosition];
    if (nextPatternIndex < 0 || nextPatternIndex >= (int) song.patterns.size())
        nextPatternIndex = 0;
    currentPatternIndex = nextPatternIndex;

    transport.setPatternRows(song.patterns[(size_t) currentPatternIndex].rows);
    const int swing = (currentPatternIndex < (int) song.patternSwing.size())
        ? song.patternSwing[(size_t) currentPatternIndex] : 50;
    transport.setSwingPercent(swing);
    transport.jumpToRow(0);
}

void XtpSequencerSource::advanceOneTick()
{
    const int previousRow = transport.row();
    const int previousPatternRows = song.patterns[(size_t) currentPatternIndex].rows;

    const bool newRow = transport.advanceTick();
    if (newRow)
    {
        if (transport.row() == 0 && previousRow == previousPatternRows - 1)
            advanceSongOrder();

        if (finished)
            return;

        sequencer.onRowBoundary(song.patterns[(size_t) currentPatternIndex], transport, *this);
    }

    sequencer.onTick(song.patterns[(size_t) currentPatternIndex], transport, *this);

    currentRowForUi.store(transport.row());
    currentPatternForUi.store(currentPatternIndex);
}

void XtpSequencerSource::prepareToPlay(int, double) {}
void XtpSequencerSource::releaseResources() {}

void XtpSequencerSource::renderChunk(int destOffset, int numSamples)
{
    voiceScratch.assign((size_t) numSamples, 0.0f);

    for (auto& v : instrumentVoices)
        if (v && v->hasActiveVoices())
            v->renderAdd(voiceScratch, sampleRate);
    for (auto& [slot, v] : sampleVoices)
        if (v && v->hasActiveVoices())
            v->renderAdd(voiceScratch, sampleRate);

    for (int i = 0; i < numSamples; ++i)
        monoScratch[(size_t) (destOffset + i)] += voiceScratch[(size_t) i];
}

void XtpSequencerSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    const int numSamples = info.numSamples;

    if (!isValid())
    {
        info.clearActiveBufferRegion();
        return;
    }

    monoScratch.assign((size_t) numSamples, 0.0f);

    int samplesRendered = 0;
    while (samplesRendered < numSamples && !finished)
    {
        const double secondsRemainingInTick = transport.secondsUntilNextTick() - tickTimeAccumulator;
        juce::int64 samplesUntilNextTick = (juce::int64) std::ceil(secondsRemainingInTick * sampleRate);
        samplesUntilNextTick = juce::jmax((juce::int64) 1, samplesUntilNextTick);

        const int samplesThisChunk = (int) juce::jmin((juce::int64) (numSamples - samplesRendered), samplesUntilNextTick);

        renderChunk(samplesRendered, samplesThisChunk);

        tickTimeAccumulator += (double) samplesThisChunk / sampleRate;
        samplesRendered += samplesThisChunk;
        position += samplesThisChunk;

        if (tickTimeAccumulator >= transport.secondsUntilNextTick() - 1e-9)
        {
            tickTimeAccumulator = 0.0;
            advanceOneTick();
        }
    }

    for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
    {
        float* dest = info.buffer->getWritePointer(ch, info.startSample);
        for (int i = 0; i < numSamples; ++i)
            dest[i] = monoScratch[(size_t) i] * kCenterGain;
    }
}

void XtpSequencerSource::setNextReadPosition(juce::int64 newPosition)
{
    if (!isValid())
        return;

    if (newPosition <= 0)
    {
        seekToSampleZero();
        return;
    }

    if (newPosition == position)
        return;

    if (newPosition < position)
        seekToSampleZero();

    // Coarse seek: replay the sequencer's transport/effect state silently (no
    // voices are actually triggered) up to the target time. This gets the
    // pattern/row/tempo state exactly right but does not re-synthesize the
    // audio that played along the way -- an accepted approximation for a live
    // sequencer, which (unlike a fixed PCM stream) can't be block-sought.
    suppressAudibleDispatch = true;
    constexpr juce::int64 kMaxSimulatedTicks = 5'000'000; // generous safety cap against a pathological request
    juce::int64 simulatedTicks = 0;
    while (position < newPosition && !finished && simulatedTicks < kMaxSimulatedTicks)
    {
        const double secondsLeftInTick = transport.secondsUntilNextTick() - tickTimeAccumulator;
        const juce::int64 samplesLeftInTick = juce::jmax((juce::int64) 1, (juce::int64) std::ceil(secondsLeftInTick * sampleRate));
        const juce::int64 samplesToTarget = newPosition - position;
        const juce::int64 step = juce::jmax((juce::int64) 1, juce::jmin(samplesLeftInTick, samplesToTarget));

        tickTimeAccumulator += (double) step / sampleRate;
        position += step;

        if (tickTimeAccumulator >= transport.secondsUntilNextTick() - 1e-9)
        {
            tickTimeAccumulator = 0.0;
            advanceOneTick();
            ++simulatedTicks;
        }
    }
    suppressAudibleDispatch = false;
}
