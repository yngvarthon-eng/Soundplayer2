#pragma once

#if defined(SOUNDPLAYER_FLUIDSYNTH_SUPPORT)

#include "XtpInstrumentVoice.h"
#include <juce_core/juce_core.h>
#include <fluidsynth.h>

namespace xtp
{

// SoundFont (.sf2) instrument playback via FluidSynth, for INSTRUMENT_ASSIGN
// slots using the "sf2:<path>" scheme. Mirrors exTracker's own SF2Plugin
// settings (extracker/src/plugin_host.cpp) for playback parity: polyphony 64,
// FluidSynth's own reverb/chorus disabled (exTracker never relies on them --
// only raw dry rendering), default preset 0/0 falling back to the soundfont's
// first available preset (common for drum-only banks stored at bank 128,
// which also gets marked as a percussion channel).
class Sf2Voice : public InstrumentVoice
{
public:
    Sf2Voice();
    ~Sf2Voice() override;

    // sampleRate must match the engine's render rate (XtpSequencerSource::
    // kRenderSampleRate) -- FluidSynth needs it fixed at synth-creation time.
    bool loadSoundFont(const juce::File& file, double sampleRate);
    bool isLoaded() const { return loaded; }

    void noteOn(int midiNote, std::uint8_t velocity, bool retrigger) override;
    void noteOff(int midiNote) override;
    void allNotesOff() override;
    void renderAdd(std::vector<float>& monoBuffer, double sampleRate) override;
    bool hasActiveVoices() const override;

private:
    fluid_settings_t* settings = nullptr;
    fluid_synth_t* synth = nullptr;
    bool loaded = false;

    std::vector<float> scratchLeft, scratchRight;

    static constexpr float kGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sf2Voice)
};

} // namespace xtp

#endif // SOUNDPLAYER_FLUIDSYNTH_SUPPORT
