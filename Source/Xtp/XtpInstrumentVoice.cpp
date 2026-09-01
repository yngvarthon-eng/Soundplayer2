#include "XtpInstrumentVoice.h"

#include <algorithm>
#include <cmath>

namespace xtp
{

namespace
{
    constexpr double kTwoPi = 6.28318530717958647692;

    double midiNoteToFrequencyHz(int midiNote)
    {
        const int clamped = std::clamp(midiNote, 0, 127);
        return 440.0 * std::pow(2.0, (double) (clamped - 69) / 12.0);
    }
}

// ---- BuiltinOscillatorVoice ----

void BuiltinOscillatorVoice::noteOn(int midiNote, std::uint8_t velocity, bool retrigger)
{
    const double target = std::clamp((double) velocity / 127.0, 0.0, 1.0) * gain;
    if (target <= 0.0)
        return;

    for (auto& v : voices)
    {
        if (v.midiNote == midiNote)
        {
            v.frequencyHz = midiNoteToFrequencyHz(midiNote);
            v.targetLevel = target;
            v.releasing = false;
            if (retrigger)
            {
                v.phase = 0.0;
                v.level = 0.0;
            }
            return;
        }
    }

    Voice v;
    v.midiNote = midiNote;
    v.frequencyHz = midiNoteToFrequencyHz(midiNote);
    v.targetLevel = target;
    voices.push_back(v);
}

void BuiltinOscillatorVoice::noteOff(int midiNote)
{
    for (auto& v : voices)
        if (v.midiNote == midiNote)
            v.releasing = true;
}

void BuiltinOscillatorVoice::allNotesOff()
{
    for (auto& v : voices)
        v.releasing = true;
}

void BuiltinOscillatorVoice::renderAdd(std::vector<float>& monoBuffer, double sampleRate)
{
    if (monoBuffer.empty() || voices.empty() || sampleRate <= 0.0)
        return;

    const double attackStep  = 1.0 / std::max(sampleRate * (attackMs / 1000.0), 1.0);
    const double releaseStep = 1.0 / std::max(sampleRate * (releaseMs / 1000.0), 1.0);

    for (std::size_t frame = 0; frame < monoBuffer.size(); ++frame)
    {
        double mixed = 0.0;
        for (auto& v : voices)
        {
            const double phaseIncrement = kTwoPi * std::max(v.frequencyHz, 1.0) / sampleRate;
            v.phase += phaseIncrement;
            if (v.phase >= kTwoPi)
                v.phase -= kTwoPi;

            if (v.releasing)
                v.level = std::max(0.0, v.level - releaseStep);
            else
                v.level = std::min(v.targetLevel, v.level + attackStep);

            const double s = (waveform == Waveform::Sine)
                ? std::sin(v.phase)
                : (std::sin(v.phase) >= 0.0 ? 1.0 : -1.0);
            mixed += s * v.level;
        }

        mixed /= (double) voices.size();
        monoBuffer[frame] += (float) mixed;

        voices.erase(std::remove_if(voices.begin(), voices.end(),
                        [](const Voice& v) { return v.releasing && v.level <= 0.0; }),
                     voices.end());
        if (voices.empty())
            break;
    }
}

// ---- SampleVoice ----

bool SampleVoice::loadSample(const juce::File& file)
{
    // file.createInputStream() returns nullptr for a missing/unreadable file (stale
    // path, moved project, etc.) -- WavAudioFormatReader's constructor dereferences its
    // stream unconditionally on the first line with no null check, so that nullptr has
    // to be caught here rather than handed straight to createReaderFor().
    auto stream = file.createInputStream();
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader(wavFormat.createReaderFor(stream.release(), true));
    if (reader == nullptr)
        return false;

    const int numChannels = (int) reader->numChannels;
    const int numFrames = (int) reader->lengthInSamples;
    if (numChannels <= 0 || numFrames <= 0)
        return false;

    juce::AudioBuffer<float> buffer(numChannels, numFrames);
    reader->read(&buffer, 0, numFrames, 0, true, true);

    // Multi-channel WAV is downmixed to mono by simple averaging, matching
    // exTracker's own loader.
    mono.assign((std::size_t) numFrames, 0.0f);
    for (int frame = 0; frame < numFrames; ++frame)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            sum += buffer.getSample(ch, frame);
        mono[(std::size_t) frame] = sum / (float) numChannels;
    }

    sampleRate = reader->sampleRate;
    voices.clear();
    return true;
}

void SampleVoice::noteOn(int midiNote, std::uint8_t velocity, bool retrigger)
{
    if (mono.empty() || sampleRate <= 0.0)
        return;

    const double vel = std::clamp((double) velocity / 127.0, 0.0, 1.0);
    for (auto& v : voices)
    {
        if (v.midiNote == midiNote)
        {
            // Note: matches exTracker exactly -- `releasing`/`envelope` are NOT
            // reset here, only pos/level/pitch. Re-triggering the same key while
            // its previous release tail is still ticking down will keep fading
            // toward silence despite the new noteOn; this is a quirk of the
            // reference engine, reproduced for playback parity.
            v.active = true;
            if (retrigger)
                v.pos = 0.0;
            v.level = vel * gain;
            v.pitchRatio = std::pow(2.0, (double) (midiNote - rootMidiNote) / 12.0);
            return;
        }
    }

    // Per-tick re-issues (retrigger=false) must not restart a finished sample — only
    // an explicit row-boundary trigger (retrigger=true) should start a new playback.
    if (!retrigger)
        return;

    Voice v;
    v.midiNote = midiNote;
    v.level = vel * gain;
    v.pitchRatio = std::pow(2.0, (double) (midiNote - rootMidiNote) / 12.0);
    voices.push_back(v);
}

void SampleVoice::noteOff(int midiNote)
{
    for (auto& v : voices)
        if (v.midiNote == midiNote)
            v.releasing = true;
}

void SampleVoice::allNotesOff()
{
    voices.clear();
}

void SampleVoice::renderAdd(std::vector<float>& monoBuffer, double outputSampleRate)
{
    if (monoBuffer.empty() || outputSampleRate <= 0.0 || mono.empty())
        return;

    const double baseStep = sampleRate / outputSampleRate;
    const double releaseStep = 1.0 / std::max(outputSampleRate * 0.03, 1.0);
    const std::size_t sampleSize = mono.size();

    for (std::size_t frame = 0; frame < monoBuffer.size(); ++frame)
    {
        double mixed = 0.0;
        std::size_t activeCount = 0;

        for (auto& v : voices)
        {
            if (!v.active)
                continue;

            if (v.pos < 0.0)
                v.pos = 0.0;
            const std::size_t idx = (std::size_t) v.pos;
            if (idx >= sampleSize)
            {
                v.active = false;
                continue;
            }

            const std::size_t nextIdx = std::min(idx + 1, sampleSize - 1);
            const double frac = v.pos - (double) idx;
            const double sampleValue = (double) mono[idx] * (1.0 - frac) + (double) mono[nextIdx] * frac;

            if (v.releasing)
                v.envelope = std::max(0.0, v.envelope - releaseStep);
            else
                v.envelope = 1.0;

            mixed += sampleValue * v.level * v.envelope;
            v.pos += baseStep * v.pitchRatio; // one-shot: no loop wrap
            if (v.envelope <= 0.0)
                v.active = false;
            ++activeCount;
        }

        if (activeCount > 0)
            monoBuffer[frame] += (float) (mixed / (double) activeCount);
    }

    voices.erase(std::remove_if(voices.begin(), voices.end(),
                    [](const Voice& v) { return !v.active; }),
                 voices.end());
}

} // namespace xtp
