#include "XtpSequencer.h"

#include <algorithm>
#include <cmath>

namespace xtp
{

namespace
{
    // Port of exTracker's free waveformSample() (sequencer.cpp), used for the
    // tremolo LFO. Phase is in radians; only the low 2 bits of `waveform` select
    // sine(0)/rising-saw(1)/square(2)/triangle(3).
    double waveformSample(std::uint8_t waveform, double phase)
    {
        constexpr double kPi = 3.14159265358979323846;
        constexpr double kTwoPi = 2.0 * kPi;

        double wrapped = std::fmod(phase, kTwoPi);
        if (wrapped < 0.0)
            wrapped += kTwoPi;
        const double normalized = wrapped / kTwoPi;

        switch (waveform & 0x03)
        {
            case 0x1: return (2.0 * normalized) - 1.0;
            case 0x2: return wrapped < kPi ? 1.0 : -1.0;
            case 0x3: return 1.0 - (4.0 * std::abs(normalized - 0.5));
            case 0x0:
            default:  return std::sin(phase);
        }
    }
}

void Sequencer::reset(int numChannels)
{
    channelControl.assign((size_t) std::max(0, numChannels), ChannelControlState {});
    channelNote.assign((size_t) std::max(0, numChannels), ChannelNote {});
    channelVolume.assign((size_t) std::max(0, numChannels), 1.0f);
    rowDelayTargetRow = kInvalidLoopRow;
    rowDelayRowsRemaining = 0;
}

void Sequencer::setChannelVolumes(const std::vector<float>& volumes)
{
    for (size_t ch = 0; ch < channelVolume.size() && ch < volumes.size(); ++ch)
        channelVolume[ch] = volumes[ch];
}

std::uint8_t Sequencer::effectiveVelocity(int channel, const ChannelNote& note, std::uint8_t velocity) const
{
    std::uint8_t vel = velocity;
    if (note.legacyFilterEnabled)
    {
        const int scaled = (int) std::lround((double) vel * 0.75);
        vel = (std::uint8_t) std::clamp(scaled, 1, 127);
    }
    if (channel >= 0 && (size_t) channel < channelVolume.size() && channelVolume[(size_t) channel] != 1.0f)
    {
        const int scaled = (int) std::lround((double) vel * (double) channelVolume[(size_t) channel]);
        vel = (std::uint8_t) std::clamp(scaled, 1, 127);
    }
    return vel;
}

void Sequencer::dispatchNoteOn(int channel, ChannelNote& note, NoteSink& sink, bool retrigger)
{
    const std::uint8_t vel = effectiveVelocity(channel, note, note.velocity);
    note.currentVelocity = vel;
    sink.noteOn(note.instrument, note.sample, note.midiNote, vel, retrigger);
}

bool Sequencer::dispatchRow(std::uint32_t row, const Pattern& pattern, TransportClock& transport, NoteSink& sink)
{
    if (channelControl.size() != (size_t) pattern.channels)
        reset(pattern.channels);

    std::uint32_t pendingJumpRow = row;
    bool hasPendingJump = false;

    for (int channel = 0; channel < pattern.channels; ++channel)
    {
        auto& cs = channelControl[(size_t) channel];
        auto& ns = channelNote[(size_t) channel];

        const Step& step = pattern.at((int) row, channel);
        std::uint8_t effectCommand = step.effectCommand;
        std::uint8_t effectValue = step.effectValue;

        // Effect memory: continuous commands (0x00-0x0A) auto-carry across blank
        // cells until explicitly replaced; every command remembers its last
        // non-zero value so a repeated command with value 0 reuses it.
        if (effectCommand == 0 && effectValue == 0)
        {
            if (cs.lastContinuousCommand != kNoContinuousCommand && cs.lastContinuousCommand <= 0x0A)
            {
                effectCommand = cs.lastContinuousCommand;
                effectValue = cs.effectMemory[effectCommand];
            }
        }
        else if (effectCommand <= 0x0A)
        {
            cs.lastContinuousCommand = effectCommand;
        }
        else
        {
            cs.lastContinuousCommand = kNoContinuousCommand;
        }

        if (effectCommand < cs.effectMemory.size())
        {
            if (effectValue == 0)
                effectValue = cs.effectMemory[effectCommand];
            else
                cs.effectMemory[effectCommand] = effectValue;
        }

        if ((effectCommand == 0x05 || effectCommand == 0x06) && effectValue == 0)
            effectValue = cs.effectMemory[0x0A];
        if ((effectCommand == 0x05 || effectCommand == 0x06) && effectValue != 0)
            cs.effectMemory[0x0A] = effectValue;

        switch (effectCommand)
        {
            case 0x0F:
                if (effectValue > 0 && effectValue < 32)
                    transport.setTicksPerRow(effectValue);
                else if (effectValue >= 32)
                    transport.setTempoBpm((double) effectValue);
                break;
            case 0x0B:
            case 0x0D:
                // Identical behavior for both commands in exTracker itself: a
                // same-pattern row jump. 0x0D looks like "pattern break" but never
                // advances to a different pattern -- reproduced as-is for parity.
                hasPendingJump = true;
                pendingJumpRow = effectValue;
                break;
            case 0x17:
                if (effectValue > 0)
                    transport.setTicksPerBeat(effectValue);
                break;
            default:
                break;
        }

        if (effectCommand == 0x0E)
        {
            const std::uint8_t sub = (std::uint8_t) ((effectValue >> 4) & 0x0F);
            const std::uint8_t subVal = (std::uint8_t) (effectValue & 0x0F);
            switch (sub)
            {
                case 0x0: // legacy filter toggle
                    cs.legacyFilterEnabled = subVal != 0;
                    break;
                case 0x6: // pattern loop
                {
                    if (subVal == 0)
                    {
                        cs.loopStartRow = row;
                    }
                    else
                    {
                        std::uint32_t loopStart = cs.loopStartRow;
                        if (loopStart >= (std::uint32_t) pattern.rows)
                        {
                            loopStart = 0;
                            cs.loopStartRow = 0;
                        }
                        if (cs.loopRemaining == 0)
                        {
                            if (cs.loopEndRow == row)
                                cs.loopEndRow = kInvalidLoopRow;
                            else
                            {
                                cs.loopRemaining = subVal;
                                cs.loopEndRow = row;
                            }
                        }
                        if (cs.loopRemaining > 0)
                        {
                            hasPendingJump = true;
                            pendingJumpRow = loopStart;
                            cs.loopRemaining = (std::uint8_t) (cs.loopRemaining - 1);
                        }
                    }
                    break;
                }
                case 0x7: // tremolo waveform
                    cs.tremoloWaveform = (std::uint8_t) (subVal & 0x03);
                    break;
                case 0xE: // row delay
                    if (subVal > 0)
                    {
                        rowDelayTargetRow = row;
                        rowDelayRowsRemaining = subVal;
                    }
                    break;
                case 0xF: // funk repeat
                    cs.funkRepeatTicks = subVal;
                    break;
                default:
                    break;
            }
        }

        // Note-off cell ("^^^").
        if (step.hasNote && step.note < 0)
        {
            if (ns.active)
            {
                if (effectCommand == 0x14)
                {
                    ns.fadingOut = true;
                    ns.fadeDecrement = effectValue > 0 ? effectValue : (std::uint8_t) 8;
                }
                else
                {
                    sink.noteOff(ns.instrument, ns.sample, ns.midiNote);
                    ns.active = false;
                    ns.fadingOut = false;
                    ns.releasedByGate = true;
                }
            }
            continue;
        }

        if (!step.hasNote)
        {
            // Cxx (set volume) on an empty cell: carry the sustaining note forward
            // at the new velocity without retriggering, so chord/pad fades work.
            if (effectCommand == 0x0C && effectValue > 0 && ns.active && !ns.releasedByGate)
            {
                ns.velocity = (std::uint8_t) std::clamp((int) effectValue, 1, 127);
                ns.gateTicks = 0;
                ns.retriggerTicks = 0;
                ns.noteCutTicks = 0;
                ns.noteDelayTicks = 0;
                ns.hasStarted = true;
                ns.delayedStart = false;
                ns.legacyFilterEnabled = cs.legacyFilterEnabled;
                dispatchNoteOn(channel, ns, sink, false);
            }
            continue;
        }

        // A real note trigger.
        const int midiNote = step.note;
        const std::uint8_t instrument = step.instrument;
        const std::uint16_t sample = step.sample;

        std::uint32_t gate = step.gateTicks;
        std::uint8_t velocity = step.velocity;
        if (effectCommand == 0x0C)
            velocity = (std::uint8_t) std::clamp((int) effectValue, 1, 127);

        int volumeSlideDelta = 0;
        if (effectCommand == 0x0A || effectCommand == 0x05 || effectCommand == 0x06)
        {
            const int up = (effectValue >> 4) & 0x0F;
            const int down = effectValue & 0x0F;
            volumeSlideDelta = up - down;
        }

        std::uint8_t tremoloSpeed = 0, tremoloDepth = 0;
        if (effectCommand == 0x07)
        {
            tremoloSpeed = (std::uint8_t) ((effectValue >> 4) & 0x0F);
            tremoloDepth = (std::uint8_t) (effectValue & 0x0F);
        }

        std::uint8_t retriggerTicks = (effectCommand == 0x09) ? effectValue : (std::uint8_t) 0;
        std::uint8_t noteCutTicks = 0, noteDelayTicks = 0;
        int fineVolumeUp = 0, fineVolumeDown = 0;
        bool legacyFilterEnabled = cs.legacyFilterEnabled;
        std::uint8_t tremoloWaveform = cs.tremoloWaveform;

        if (effectCommand == 0x0E)
        {
            const std::uint8_t sub = (std::uint8_t) ((effectValue >> 4) & 0x0F);
            const std::uint8_t subVal = (std::uint8_t) (effectValue & 0x0F);
            switch (sub)
            {
                case 0x9: retriggerTicks = subVal; break;
                case 0xA: fineVolumeUp = subVal; break;
                case 0xB: fineVolumeDown = subVal; break;
                case 0x0: legacyFilterEnabled = subVal != 0; cs.legacyFilterEnabled = legacyFilterEnabled; break;
                case 0x7: tremoloWaveform = (std::uint8_t) (subVal & 0x03); cs.tremoloWaveform = tremoloWaveform; break;
                case 0xC: noteCutTicks = subVal; break;
                case 0xD: noteDelayTicks = subVal; break;
                case 0xF: retriggerTicks = subVal; cs.funkRepeatTicks = subVal; break;
                default: break;
            }
        }

        if (retriggerTicks == 0 && cs.funkRepeatTicks > 0)
            retriggerTicks = cs.funkRepeatTicks;

        if (fineVolumeUp > 0 || fineVolumeDown > 0)
            velocity = (std::uint8_t) std::clamp((int) velocity + fineVolumeUp - fineVolumeDown, 1, 127);

        // Cut whatever was previously sustained on this channel if this is a
        // genuinely different note/instrument/sample (not a same-note continuation).
        if (ns.active && (ns.midiNote != midiNote || ns.instrument != instrument || ns.sample != sample))
        {
            sink.noteOff(ns.instrument, ns.sample, ns.midiNote);
            ns.active = false;
        }

        ns.midiNote = midiNote;
        ns.instrument = instrument;
        ns.sample = sample;
        ns.gateTicks = gate;
        ns.velocity = velocity;
        ns.volumeSlideDelta = volumeSlideDelta;
        ns.tremoloSpeed = tremoloSpeed;
        ns.tremoloDepth = tremoloDepth;
        ns.tremoloWaveform = tremoloWaveform;
        ns.legacyFilterEnabled = legacyFilterEnabled;
        ns.retriggerTicks = retriggerTicks;
        ns.noteCutTicks = noteCutTicks;
        ns.noteDelayTicks = noteDelayTicks;
        ns.delayedStart = noteDelayTicks > 0;
        ns.hasStarted = noteDelayTicks == 0;
        ns.lastRetriggerTick = 0;
        ns.tremoloPhase = 0.0;
        ns.releasedByGate = false;
        ns.fadingOut = false;
        ns.active = true;

        if (!ns.delayedStart)
            dispatchNoteOn(channel, ns, sink, true); // row boundary always retriggers
    }

    if (hasPendingJump)
    {
        transport.jumpToRow((int) pendingJumpRow);
        return true;
    }
    return false;
}

void Sequencer::onRowBoundary(const Pattern& pattern, TransportClock& transport, NoteSink& sink)
{
    if (channelControl.size() != (size_t) pattern.channels)
        reset(pattern.channels);

    std::uint32_t row = (std::uint32_t) transport.row();

    if (rowDelayRowsRemaining > 0 && rowDelayTargetRow != kInvalidLoopRow && row != rowDelayTargetRow)
    {
        transport.jumpToRow((int) rowDelayTargetRow);
        --rowDelayRowsRemaining;
        row = (std::uint32_t) transport.row();
    }
    else if (rowDelayRowsRemaining == 0 && row != rowDelayTargetRow)
    {
        rowDelayTargetRow = kInvalidLoopRow;
    }

    // 0x0B/0x0D/E6x cause a same-tick re-dispatch of the row jumped to (zero
    // elapsed time); chase that chain to completion, with a safety cap against
    // a pathological pattern that jumps to itself forever.
    constexpr int kMaxChainedJumps = 256;
    int iterations = 0;
    bool jumped = dispatchRow(row, pattern, transport, sink);
    while (jumped && ++iterations < kMaxChainedJumps)
    {
        row = (std::uint32_t) transport.row();
        jumped = dispatchRow(row, pattern, transport, sink);
    }
}

void Sequencer::onTick(const Pattern& pattern, TransportClock& transport, NoteSink& sink)
{
    if (channelControl.size() != (size_t) pattern.channels)
        return;

    const std::uint32_t ticksIntoRow = transport.ticksIntoRow();
    if (ticksIntoRow == 0)
        return; // the row's first tick is handled by dispatchRow's trigger, not here

    for (int channel = 0; channel < pattern.channels; ++channel)
    {
        auto& ns = channelNote[(size_t) channel];
        if (!ns.active || ns.releasedByGate)
            continue;

        if (!ns.hasStarted)
        {
            if (ns.noteDelayTicks > 0 && ticksIntoRow >= ns.noteDelayTicks)
            {
                ns.hasStarted = true;
                ns.delayedStart = false;
                dispatchNoteOn(channel, ns, sink, true);
            }
            else
            {
                continue;
            }
        }

        if (ns.gateTicks > 0 && ticksIntoRow >= ns.gateTicks)
        {
            sink.noteOff(ns.instrument, ns.sample, ns.midiNote);
            ns.releasedByGate = true;
            continue;
        }

        if (ns.noteCutTicks > 0 && ticksIntoRow >= ns.noteCutTicks)
        {
            sink.noteOff(ns.instrument, ns.sample, ns.midiNote);
            ns.releasedByGate = true;
            continue;
        }

        if (ns.retriggerTicks > 0 && ticksIntoRow % ns.retriggerTicks == 0 && ns.lastRetriggerTick != ticksIntoRow)
        {
            dispatchNoteOn(channel, ns, sink, true);
            ns.lastRetriggerTick = ticksIntoRow;
        }

        if (ns.volumeSlideDelta != 0)
        {
            const int updated = std::clamp((int) ns.velocity + ns.volumeSlideDelta, 1, 127);
            ns.velocity = (std::uint8_t) updated;
        }

        std::uint8_t outputVelocity = ns.velocity;
        if (ns.tremoloSpeed > 0 && ns.tremoloDepth > 0)
        {
            ns.tremoloPhase += (double) ns.tremoloSpeed * 0.25;
            const double lfo = waveformSample(ns.tremoloWaveform, ns.tremoloPhase);
            const double amount = lfo * (double) ns.tremoloDepth * 0.06;
            const int scaled = (int) std::lround((double) ns.velocity * (1.0 + amount));
            outputVelocity = (std::uint8_t) std::clamp(scaled, 1, 127);
        }

        // Unconditional per-tick re-issue (retrigger=false), matching exTracker's
        // own behavior of refreshing every held note every tick.
        const std::uint8_t vel = effectiveVelocity(channel, ns, outputVelocity);
        sink.noteOn(ns.instrument, ns.sample, ns.midiNote, vel, false);
    }

    for (auto& ns : channelNote)
    {
        if (!ns.fadingOut || !ns.active)
            continue;

        if (ns.currentVelocity <= ns.fadeDecrement)
        {
            ns.active = false;
            ns.fadingOut = false;
            sink.noteOff(ns.instrument, ns.sample, ns.midiNote);
        }
        else
        {
            ns.currentVelocity = (std::uint8_t) (ns.currentVelocity - ns.fadeDecrement);
            sink.noteOn(ns.instrument, ns.sample, ns.midiNote, ns.currentVelocity, false);
        }
    }
}

void Sequencer::allNotesOff(NoteSink& sink)
{
    for (auto& ns : channelNote)
    {
        if (ns.active)
        {
            sink.noteOff(ns.instrument, ns.sample, ns.midiNote);
            ns.active = false;
            ns.fadingOut = false;
        }
    }
}

} // namespace xtp
