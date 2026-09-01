#pragma once

#include "XtpSong.h"
#include "XtpTransportClock.h"
#include <array>
#include <cstdint>
#include <limits>

namespace xtp
{

// Receives resolved note events from XtpSequencer. Implemented by
// XtpSequencerSource, which maps (instrument, sample) to an InstrumentVoice
// using the same resolution order as exTracker's
// PluginHost::triggerNoteOnResolved/triggerNoteOffResolved: explicit sample
// slot first, then legacy instrument>=16-as-sample-slot addressing, then the
// instrument slot's assigned plugin, then (if that slot is unassigned) a
// same-numbered sample slot.
class NoteSink
{
public:
    virtual ~NoteSink() = default;
    virtual void noteOn(std::uint8_t instrument, std::uint16_t sample, int midiNote,
                        std::uint8_t velocity, bool retrigger) = 0;
    virtual void noteOff(std::uint8_t instrument, std::uint16_t sample, int midiNote) = 0;
};

// Row/tick effect dispatch engine. Ports the control-flow and formulas of
// exTracker's Sequencer::update (extracker/src/sequencer.cpp) with one
// deliberate scope reduction: effects that only modulate *continuous*
// frequency (arpeggio 0x00, slide 0x01/0x02, portamento 0x03/0x05-partial,
// vibrato 0x04/0x06-partial, fine slide E1x/E2x, glissando E3x, finetune E5x)
// are parsed but not applied.
//
// This isn't a shortcut -- it matches exTracker's *actual* audible behavior.
// Its instrument-plugin interface (PluginHost::triggerNoteOnResolved) only
// accepts a discrete MIDI note, never a continuous frequency; only the unused
// fallback tone-generator path (taken solely when an instrument slot has no
// plugin assigned, which doesn't happen with the default builtin.sine/
// builtin.square instruments) receives the continuously-modulated frequency.
// So on every normally-configured instrument, those effects already produce no
// audible pitch change in the reference engine itself. Reproducing them here
// would make songs sound *more* expressive than they do in exTracker, which is
// the opposite of the compatibility goal. Effects that modulate velocity
// (tremolo 0x07/E7x, volume slide 0x0A and the volume-slide nibble of
// 0x05/0x06, set-volume 0x0C, fine volume EAx/EBx) go through the velocity
// parameter, which the plugin interface does accept, so those work normally.
// Pan (0x08/E8x) has the same "no-op for plugin instruments" issue in
// exTracker and is likewise parsed but not applied here.
class Sequencer
{
public:
    void reset(int numChannels);

    // Per-channel volume multiplier (0.0-2.0, 1.0 = unity), loaded from the
    // song file's CHANNEL_VOLUME trailer token (see XtpSong.cpp) and applied
    // at note-trigger time, matching exTracker's Sequencer::channelVolume_
    // (src/sequencer.cpp) -- scales trigger velocity, clamped back to 1-127.
    // Entries beyond `volumes.size()` keep the default of 1.0.
    void setChannelVolumes(const std::vector<float>& volumes);

    // Call once whenever the transport crosses into a new row (including a
    // forced re-entry after a same-tick jump). `pattern` is the pattern
    // currently playing. Returns true if this row's effects caused a same-tick
    // jump (transport.jumpToRow() was already called internally) -- the caller
    // should re-invoke dispatchRow for the transport's new row when this
    // happens, which is what onRowBoundary() below does for you.
    bool dispatchRow(std::uint32_t row, const Pattern& pattern, TransportClock& transport, NoteSink& sink);

    // Full row-boundary handling: row-delay (EEx) redirection, then
    // dispatchRow(), then following same-tick jump chains (0x0B/0x0D/E6x) to
    // completion. Call this once per row entered, from XtpSequencerSource.
    void onRowBoundary(const Pattern& pattern, TransportClock& transport, NoteSink& sink);

    // Call once per tick (including the tick a row boundary was just dispatched
    // on) for per-tick effects: gate/note-cut/note-delay timing, retrigger,
    // volume slide, tremolo, and 0x14 fade-out.
    void onTick(const Pattern& pattern, TransportClock& transport, NoteSink& sink);

    // Immediately silences and clears all channel state, calling sink.noteOff
    // for anything currently sounding. Call before a seek/rewind.
    void allNotesOff(NoteSink& sink);

private:
    static constexpr std::uint32_t kInvalidLoopRow = std::numeric_limits<std::uint32_t>::max();
    static constexpr std::uint8_t kNoContinuousCommand = 0xFF;

    // Per-channel row-boundary control state (persists across rows).
    struct ChannelControlState
    {
        std::array<std::uint8_t, 16> effectMemory {};
        std::uint8_t lastContinuousCommand = kNoContinuousCommand;
        bool legacyFilterEnabled = false;      // E0x: scales trigger velocity by 0.75 while enabled
        std::uint8_t funkRepeatTicks = 0;      // EFx
        std::uint32_t loopStartRow = 0;        // E6x
        std::uint8_t loopRemaining = 0;
        std::uint32_t loopEndRow = kInvalidLoopRow;
        std::uint8_t tremoloWaveform = 0;      // E7x: 0=sine,1=ramp,2=square,3=triangle
    };

    // The note currently sounding (or just triggered) on a channel. One per
    // channel, replacing exTracker's separate activeNotes_/currentRowNotes_/
    // channelNoteState_ -- a valid simplification here because this format's
    // one-Step-per-(row,channel) cell model can never produce more than one
    // explicitly-placed note per channel per row (unlike exTracker's own
    // multi-RowNote merge logic, which exists for edge cases that model can't
    // produce in the first place).
    struct ChannelNote
    {
        bool active = false;         // a voice is currently on (or fading out)
        bool hasStarted = false;     // false while waiting on a note-delay (EDx)
        bool delayedStart = false;
        bool releasedByGate = false; // gate/note-cut already released this note this row
        bool fadingOut = false;      // 0x14 fade-out in progress
        std::uint8_t fadeDecrement = 0;
        std::uint8_t currentVelocity = 100;

        int midiNote = 0;
        std::uint8_t instrument = 0;
        std::uint16_t sample = 0xFFFF;

        std::uint32_t gateTicks = 0;
        std::uint8_t velocity = 100;
        int volumeSlideDelta = 0;

        std::uint8_t tremoloSpeed = 0;
        std::uint8_t tremoloDepth = 0;
        std::uint8_t tremoloWaveform = 0;
        double tremoloPhase = 0.0;

        bool legacyFilterEnabled = false;
        std::uint8_t retriggerTicks = 0;
        std::uint8_t noteCutTicks = 0;
        std::uint8_t noteDelayTicks = 0;
        std::uint32_t lastRetriggerTick = 0;
    };

    std::vector<ChannelControlState> channelControl;
    std::vector<ChannelNote> channelNote;
    std::vector<float> channelVolume;

    std::uint32_t rowDelayTargetRow = kInvalidLoopRow;
    std::uint8_t rowDelayRowsRemaining = 0;

    void dispatchNoteOn(int channel, ChannelNote& note, NoteSink& sink, bool retrigger);
    std::uint8_t effectiveVelocity(int channel, const ChannelNote& note, std::uint8_t velocity) const;
};

} // namespace xtp
