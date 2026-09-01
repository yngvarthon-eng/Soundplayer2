#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>
#include <istream>
#include <map>
#include <vector>

// Data model and parser for exTracker's native ".xtp" song format.
//
// .xtp is a plain-text, whitespace-tokenized format that describes a tracker
// song symbolically (patterns of notes/effects, song order, tempo) rather than
// embedding audio data. Reference: exTracker's own save/load code at
// extracker/src/main.cpp (savePatternToFile / loadPatternFromFile).
namespace xtp
{

// One pattern cell. Field names/types/ranges mirror exTracker's
// PatternEditor::Step exactly (extracker/include/extracker/pattern_editor.hpp).
struct Step
{
    static constexpr int kNoteOff = -1;

    bool hasNote = false;
    int note = -1;                  // MIDI note number; -1 with hasNote=true means note-off ("^^^")
    std::uint8_t instrument = 0;    // instrument slot 0-15, or (if sample is unset) a legacy sample-slot index >= 16
    std::uint16_t sample = 0xFFFF;  // sample bank slot 0-256; 0xFFFF = unset, fall back to instrument
    std::uint32_t gateTicks = 0;    // note length in ticks; 0 = sustain until interrupted
    std::uint8_t velocity = 100;    // 1-127
    bool retrigger = false;
    std::uint8_t effectCommand = 0; // full byte (0x00-0x23 used; 0x0E is "extended", subcommand in high nibble)
    std::uint8_t effectValue = 0;

    bool isEmpty() const { return !hasNote && effectCommand == 0 && effectValue == 0; }
};

struct Pattern
{
    int rows = 0;
    int channels = 0;
    std::vector<Step> steps; // [row * channels + channel]

    const Step& at(int row, int channel) const { return steps[(size_t) (row * channels + channel)]; }
    Step&       at(int row, int channel)       { return steps[(size_t) (row * channels + channel)]; }
};

struct SampleEntry
{
    juce::String name;
    juce::File path; // resolved to an absolute path relative to the .xtp file's directory
};

// A parsed .xtp song: static data only, no playback state. Load once per file;
// XtpSequencer/XtpSequencerSource own the live playback state that walks this.
class Song
{
public:
    int rows = 0;
    int channels = 0;
    std::vector<Pattern> patterns;
    std::vector<int> songOrder;             // sequence of pattern indices; entries out of range are skipped during playback
    std::vector<std::uint8_t> patternSwing; // percent [50,75], one per pattern; index-aligned with `patterns`

    // Per-channel volume multiplier (0.0-2.0, 1.0 = unity), from the
    // CHANNEL_VOLUME trailer token. Empty (or a channel beyond its size)
    // means unity gain -- matches exTracker's own default when a file
    // predates this feature.
    std::vector<float> channelVolume;

    double tempoBpm = 125.0;
    int ticksPerBeat = 6;
    int ticksPerRow = 6;

    // Instrument slot 0-15 -> plugin id string ("builtin.sine", "builtin.square",
    // "builtin.sample", "sf2:<path>", "sfz:<path>", "lv2:<uri>", ...), empty = unassigned.
    // Slots 0 and 1 default to builtin.sine/builtin.square, matching exTracker's own
    // runtime startup state (main.cpp assigns these before any file is loaded), which a
    // file only overrides if it has its own INSTRUMENT_ASSIGN for that slot.
    std::array<juce::String, 16> instruments;

    // Sample bank slot (0-256) -> entry, populated from SAMPLE_ENTRY/SAMPLE_BANK tokens.
    std::map<int, SampleEntry> samples;

    juce::String moduleMessage;

    bool loaded = false;

    bool loadFromFile(const juce::File& file, juce::String& errorMessage);

    // Estimated total playback duration in seconds, following the song order once
    // at the file's saved tempo/ticks-per-row/swing settings. This is approximate:
    // pattern effects that change tempo/speed mid-song (0x0F, 0x17) or alter the
    // playback path (row jumps 0x0B/0x0D, pattern loop E6x) are not simulated.
    double estimateDurationSeconds() const;

private:
    bool applyTrailerToken(std::istream& in, const std::string& token, const juce::File& moduleDirectory);
    void addSampleEntry(int slot, const juce::String& name, const std::string& rawPath, const juce::File& moduleDirectory);
};

} // namespace xtp
