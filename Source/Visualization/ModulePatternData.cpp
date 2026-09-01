#include "ModulePatternData.h"

#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
 #include <libopenmpt/libopenmpt.hpp>
#endif

// ---- ModulePatternData ----

void ModulePatternData::clear()
{
    loaded      = false;
    numChannels = 0;
    title       = {};
    patterns.clear();
}

bool ModulePatternData::loadFromFile(const juce::File& file)
{
    clear();

#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    juce::MemoryBlock raw;
    if (!file.loadFileAsData(raw))
        return false;

    std::unique_ptr<openmpt::module> mod;
    try
    {
        const char* data = static_cast<const char*>(raw.getData());
        mod = std::make_unique<openmpt::module>(
            std::vector<char>(data, data + raw.getSize()), std::cerr);
    }
    catch (const openmpt::exception&)
    {
        return false;
    }

    title       = juce::String(mod->get_metadata("title"));
    numChannels = mod->get_num_channels();
    int numPats = mod->get_num_patterns();

    patterns.resize((size_t)numPats);
    for (int p = 0; p < numPats; ++p)
    {
        int numRows      = mod->get_pattern_num_rows(p);
        auto& pat        = patterns[(size_t)p];
        pat.numRows      = numRows;
        pat.numChannels  = numChannels;
        pat.notes.resize((size_t)(numRows * numChannels));

        for (int r = 0; r < numRows; ++r)
        {
            for (int c = 0; c < numChannels; ++c)
            {
                auto& dst = pat.notes[(size_t)(r * numChannels + c)];

                // Note — libopenmpt uses "..." for empty; normalise to "---"
                auto n = mod->format_pattern_row_channel_command(p, r, c,
                             openmpt::module::command_note);
                dst.note = (n == "...") ? "---" : juce::String(n);

                // Instrument — libopenmpt uses ".." for empty; normalise to "--"
                auto ins = mod->format_pattern_row_channel_command(p, r, c,
                               openmpt::module::command_instrument);
                dst.inst = (ins == "..") ? "--" : juce::String(ins);

                // Volume column: combine single-char effect type + 2-char value.
                // Both empty ("." + "..") collapses to the 2-char sentinel "..".
                // Note: libopenmpt spells this "command_volumeffect" (no 'e' before 'ffect').
                auto ve = mod->format_pattern_row_channel_command(p, r, c,
                              openmpt::module::command_volumeffect);
                auto vp = mod->format_pattern_row_channel_command(p, r, c,
                              openmpt::module::command_volume);
                dst.vol = (ve == "." && vp == "..") ? ".." : juce::String(ve + vp);

                // Effect column: combine single-char command + 2-char parameter.
                // Both empty ("." + "..") produces "..." — the 3-char sentinel.
                auto ef = mod->format_pattern_row_channel_command(p, r, c,
                              openmpt::module::command_effect);
                auto ep = mod->format_pattern_row_channel_command(p, r, c,
                              openmpt::module::command_parameter);
                dst.effect = juce::String(ef + ep);
            }
        }
    }

    loaded = !patterns.empty();
    return loaded;

#else
    juce::ignoreUnused(file);
    return false;
#endif
}

bool ModulePatternData::loadFromXtpSong(const xtp::Song& song)
{
    clear();
    if (!song.loaded || song.patterns.empty())
        return false;

    title = song.moduleMessage;
    numChannels = song.channels;

    static const char* const noteNames[12] = {
        "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
    };

    auto formatNote = [](const xtp::Step& step) -> juce::String
    {
        if (!step.hasNote)
            return "---";
        if (step.note < 0)
            return "^^^"; // note-off/cut
        const int clamped = juce::jlimit(0, 127, step.note);
        const int octave = clamped / 12 - 1;
        return juce::String(noteNames[clamped % 12]) + juce::String(octave);
    };

    auto formatInst = [](const xtp::Step& step) -> juce::String
    {
        if (!step.hasNote)
            return "--";
        return juce::String::formatted("%02d", (int) step.instrument);
    };

    auto formatVol = [](const xtp::Step& step) -> juce::String
    {
        if (!step.hasNote)
            return "..";
        return "v" + juce::String::formatted("%02d", juce::jmin(99, (int) step.velocity));
    };

    auto formatEffect = [](const xtp::Step& step) -> juce::String
    {
        if (step.effectCommand == 0 && step.effectValue == 0)
            return "...";
        return juce::String::toHexString((int) step.effectCommand).paddedLeft('0', 2).toUpperCase()
             + juce::String::toHexString((int) step.effectValue).paddedLeft('0', 2).toUpperCase();
    };

    patterns.resize(song.patterns.size());
    for (size_t p = 0; p < song.patterns.size(); ++p)
    {
        const auto& srcPat = song.patterns[p];
        auto& dstPat = patterns[p];
        dstPat.numRows = srcPat.rows;
        dstPat.numChannels = srcPat.channels;
        dstPat.notes.resize((size_t) (srcPat.rows * srcPat.channels));

        for (int r = 0; r < srcPat.rows; ++r)
        {
            for (int c = 0; c < srcPat.channels; ++c)
            {
                const auto& step = srcPat.at(r, c);
                auto& dst = dstPat.notes[(size_t) (r * srcPat.channels + c)];
                dst.note = formatNote(step);
                dst.inst = formatInst(step);
                dst.vol = formatVol(step);
                dst.effect = formatEffect(step);
            }
        }
    }

    loaded = !patterns.empty();
    return loaded;
}
