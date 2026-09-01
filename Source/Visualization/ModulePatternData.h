#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "../Xtp/XtpSong.h"

/**
 * Holds the display data for a single note cell in a tracker pattern.
 * Strings are in libopenmpt's notation, normalised so the viewer's empty
 * sentinels are consistent: "---" (note), "--" (inst), ".." (vol), "..." (effect).
 */
struct ModuleNote
{
    juce::String note;    // "C-5", "---" (empty), "^^^" (cut), "===" (key-off)
    juce::String inst;    // "01", "--" (empty)
    juce::String vol;     // "v40", ".." (empty)
    juce::String effect;  // "A01", "..." (empty)

    bool isEmpty() const { return note == "---" && inst == "--"; }

    juce::String getNoteString()   const { return note; }
    juce::String getInstString()   const { return inst; }
    juce::String getVolString()    const { return vol; }
    juce::String getEffectString() const { return effect; }
};

struct ModulePattern
{
    int numRows     = 0;
    int numChannels = 0;
    std::vector<ModuleNote> notes;  // [row * numChannels + channel]

    const ModuleNote& at(int row, int channel) const
    {
        return notes[(size_t)(row * numChannels + channel)];
    }
};

class ModulePatternData
{
public:
    ModulePatternData() = default;

    bool loadFromFile(const juce::File& file);

    // Populates from an already-parsed .xtp song (Source/Xtp/XtpSong.h), so the
    // existing pattern-viewer UI (ModulePatternViewer/PatternGridComponent) can
    // display .xtp patterns without any changes -- ModuleNote's fields are
    // already plain display strings, not libopenmpt-specific.
    bool loadFromXtpSong(const xtp::Song& song);

    void clear();

    bool isLoaded()       const { return loaded; }
    int  getNumChannels() const { return numChannels; }
    int  getNumPatterns() const { return (int)patterns.size(); }

    const ModulePattern* getPattern(int index) const
    {
        if (index >= 0 && index < (int)patterns.size())
            return &patterns[(size_t)index];
        return nullptr;
    }

    juce::String getTitle() const { return title; }

private:
    bool         loaded      = false;
    int          numChannels = 0;
    juce::String title;
    std::vector<ModulePattern> patterns;
};
