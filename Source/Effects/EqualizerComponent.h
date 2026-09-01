#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Equalizer.h"
#include "EqPresetManager.h"

class AudioEngine;

class EqualizerComponent : public juce::Component
{
public:
    EqualizerComponent(AudioEngine* engine);
    ~EqualizerComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void refreshPresetCombo();
    void applyPreset(int comboIndex);
    void onSavePreset();
    void onDeletePreset();

    AudioEngine* audioEngine;
    EqPresetManager presetManager;

    std::unique_ptr<juce::ComboBox> presetCombo;
    std::unique_ptr<juce::TextButton> saveButton;
    std::unique_ptr<juce::TextButton> deleteButton;

    std::array<std::unique_ptr<juce::Slider>, 10> bandSliders;
    std::array<std::unique_ptr<juce::Label>, 10>  bandLabels;

    std::unique_ptr<juce::Slider> volumeSlider;
    std::unique_ptr<juce::Label>  volumeLabel;

    static constexpr const char* bandNames[10] = {
        "31 Hz", "62 Hz", "125 Hz", "250 Hz", "500 Hz",
        "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqualizerComponent)
};
