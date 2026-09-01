#include "XtpSf2Voice.h"

#if defined(SOUNDPLAYER_FLUIDSYNTH_SUPPORT)

#include <cstdio>

namespace xtp
{

Sf2Voice::Sf2Voice() = default;

Sf2Voice::~Sf2Voice()
{
    if (synth != nullptr)
        delete_fluid_synth(synth);
    if (settings != nullptr)
        delete_fluid_settings(settings);
}

bool Sf2Voice::loadSoundFont(const juce::File& file, double sampleRate)
{
    settings = new_fluid_settings();
    if (settings == nullptr)
        return false;

    fluid_settings_setnum(settings, "synth.sample-rate", sampleRate);
    fluid_settings_setint(settings, "synth.polyphony", 64);
    fluid_settings_setint(settings, "synth.reverb.active", 0);
    fluid_settings_setint(settings, "synth.chorus.active", 0);

    synth = new_fluid_synth(settings);
    if (synth == nullptr)
        return false;

    const int sfontId = fluid_synth_sfload(synth, file.getFullPathName().toRawUTF8(), 1);
    if (sfontId == FLUID_FAILED)
    {
        fprintf(stderr, "Sf2Voice: failed to load soundfont '%s'\n", file.getFullPathName().toRawUTF8());
        return false;
    }

    // Default preset 0/0; if the soundfont doesn't have it (common for
    // drum-only banks stored at bank 128), fall back to its first available
    // preset -- matches exTracker's SF2Plugin.
    if (fluid_synth_program_select(synth, 0, sfontId, 0, 0) != FLUID_OK)
    {
        if (auto* sfont = fluid_synth_get_sfont_by_id(synth, sfontId))
        {
            fluid_sfont_iteration_start(sfont);
            if (auto* preset = fluid_sfont_iteration_next(sfont))
            {
                const int bank = fluid_preset_get_banknum(preset);
                const int prog = fluid_preset_get_num(preset);
                fluid_synth_program_select(synth, 0, sfontId, bank, prog);
            }
        }
    }

    if (auto* selected = fluid_synth_get_channel_preset(synth, 0))
    {
        const int bank = fluid_preset_get_banknum(selected);
        if (bank == 128)
            fluid_synth_set_channel_type(synth, 0, CHANNEL_TYPE_DRUM);
        fprintf(stderr, "Sf2Voice: '%s' -> preset '%s' (bank=%d prog=%d)\n",
                file.getFileName().toRawUTF8(), fluid_preset_get_name(selected), bank, fluid_preset_get_num(selected));
    }
    else
    {
        fprintf(stderr, "Sf2Voice: '%s' -> no preset could be selected on any channel\n", file.getFileName().toRawUTF8());
    }

    loaded = true;
    return true;
}

void Sf2Voice::noteOn(int midiNote, std::uint8_t velocity, bool /*retrigger*/)
{
    if (!loaded)
        return;
    fluid_synth_noteon(synth, 0, juce::jlimit(0, 127, midiNote), juce::jlimit(1, 127, (int) velocity));
}

void Sf2Voice::noteOff(int midiNote)
{
    if (!loaded)
        return;
    fluid_synth_noteoff(synth, 0, juce::jlimit(0, 127, midiNote));
}

void Sf2Voice::allNotesOff()
{
    if (!loaded)
        return;
    fluid_synth_all_sounds_off(synth, -1);
}

bool Sf2Voice::hasActiveVoices() const
{
    return loaded && fluid_synth_get_active_voice_count(synth) > 0;
}

void Sf2Voice::renderAdd(std::vector<float>& monoBuffer, double /*sampleRate*/)
{
    if (!loaded || monoBuffer.empty())
        return;

    const int numSamples = (int) monoBuffer.size();
    scratchLeft.assign((size_t) numSamples, 0.0f);
    scratchRight.assign((size_t) numSamples, 0.0f);

    fluid_synth_write_float(synth, numSamples,
                            scratchLeft.data(), 0, 1,
                            scratchRight.data(), 0, 1);

    const float scale = kGain * 0.5f;
    for (int i = 0; i < numSamples; ++i)
        monoBuffer[(size_t) i] += scale * (scratchLeft[(size_t) i] + scratchRight[(size_t) i]);
}

} // namespace xtp

#endif // SOUNDPLAYER_FLUIDSYNTH_SUPPORT
