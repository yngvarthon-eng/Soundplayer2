#include "EqualizerComponent.h"
#include "../Audio/AudioEngine.h"

EqualizerComponent::EqualizerComponent(AudioEngine* engine)
    : audioEngine(engine)
{
    presetCombo = std::make_unique<juce::ComboBox>();
    presetCombo->onChange = [this] {
        applyPreset(presetCombo->getSelectedItemIndex());
    };
    addAndMakeVisible(*presetCombo);

    saveButton = std::make_unique<juce::TextButton>("Save");
    saveButton->onClick = [this] { onSavePreset(); };
    addAndMakeVisible(*saveButton);

    deleteButton = std::make_unique<juce::TextButton>("Delete");
    deleteButton->onClick = [this] { onDeletePreset(); };
    addAndMakeVisible(*deleteButton);

    refreshPresetCombo();

    for (int i = 0; i < 10; ++i)
    {
        bandSliders[i] = std::make_unique<juce::Slider>(juce::Slider::LinearVertical,
                                                        juce::Slider::TextBoxBelow);
        bandSliders[i]->setRange(-12.0, 12.0, 0.5);
        bandSliders[i]->setValue(0.0);
        bandSliders[i]->onValueChange = [this, i] {
            audioEngine->getEqualizer().setGain(i, (float)bandSliders[i]->getValue());
        };
        addAndMakeVisible(*bandSliders[i]);

        bandLabels[i] = std::make_unique<juce::Label>("BandLabel", bandNames[i]);
        bandLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*bandLabels[i]);
    }

    volumeSlider = std::make_unique<juce::Slider>(juce::Slider::LinearVertical,
                                                  juce::Slider::TextBoxBelow);
    volumeSlider->setRange(0.0, 1.0, 0.01);
    volumeSlider->setValue(audioEngine->getMasterGain());
    volumeSlider->onValueChange = [this] {
        audioEngine->setMasterGain((float)volumeSlider->getValue());
    };
    addAndMakeVisible(*volumeSlider);

    volumeLabel = std::make_unique<juce::Label>("VolumeLabel", "Vol");
    volumeLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*volumeLabel);
}

EqualizerComponent::~EqualizerComponent()
{
}

void EqualizerComponent::refreshPresetCombo()
{
    presetCombo->clear(juce::dontSendNotification);
    int id = 1;
    for (const auto& preset : presetManager.getPresets())
        presetCombo->addItem(preset.name, id++);

    presetCombo->setSelectedId(1, juce::dontSendNotification);
}

void EqualizerComponent::applyPreset(int comboIndex)
{
    const auto& presets = presetManager.getPresets();
    if (comboIndex < 0 || comboIndex >= (int)presets.size())
        return;

    const auto& preset = presets[comboIndex];
    for (int i = 0; i < 10; ++i)
    {
        bandSliders[i]->setValue(preset.gains[i], juce::sendNotification);
    }

    // Update delete button — can only delete user presets
    deleteButton->setEnabled(!preset.isBuiltIn);
}

void EqualizerComponent::onSavePreset()
{
    auto* w = new juce::AlertWindow("Save EQ Preset",
                                    "Enter a name for this preset:",
                                    juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("name", "", "Preset name:");
    w->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    w->enterModalState(true,
        juce::ModalCallbackFunction::create([this, w](int result) {
            if (result == 1)
            {
                auto name = w->getTextEditorContents("name").trim();
                if (name.isEmpty())
                    return;

                std::array<float, 10> gains;
                for (int i = 0; i < 10; ++i)
                    gains[i] = (float)bandSliders[i]->getValue();

                presetManager.savePreset(name, gains);
                refreshPresetCombo();

                const auto& presets = presetManager.getPresets();
                for (int i = 0; i < (int)presets.size(); ++i)
                {
                    if (presets[i].name == name)
                    {
                        presetCombo->setSelectedItemIndex(i, juce::dontSendNotification);
                        deleteButton->setEnabled(true);
                        break;
                    }
                }
            }
        }),
        true /* deleteAfterUse */);
}

void EqualizerComponent::onDeletePreset()
{
    int idx = presetCombo->getSelectedItemIndex();
    const auto& presets = presetManager.getPresets();
    if (idx < 0 || idx >= (int)presets.size())
        return;

    auto name = presets[idx].name;
    if (presets[idx].isBuiltIn)
        return;

    presetManager.deletePreset(name);
    refreshPresetCombo();
    presetCombo->setSelectedItemIndex(0, juce::sendNotification);
}

void EqualizerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey.withAlpha(0.5f));
    g.setColour(juce::Colours::white);
    g.drawText("10-Band Equalizer", getLocalBounds().removeFromTop(20), juce::Justification::centred);

    // Separator between EQ bands and volume slider
    if (volumeSlider != nullptr)
    {
        g.setColour(juce::Colours::grey.withAlpha(0.8f));
        auto x = (float)(volumeSlider->getX() - 4);
        g.drawVerticalLine((int)x, 20.0f, (float)getHeight() - 5);
    }
}

void EqualizerComponent::resized()
{
    auto area = getLocalBounds().reduced(5);
    area.removeFromTop(20); // title

    // Preset bar
    auto presetBar = area.removeFromTop(28).reduced(0, 2);
    deleteButton->setBounds(presetBar.removeFromRight(60));
    presetBar.removeFromRight(4);
    saveButton->setBounds(presetBar.removeFromRight(60));
    presetBar.removeFromRight(4);
    presetCombo->setBounds(presetBar);

    area.removeFromTop(4); // gap

    // Reserve right side for volume slider first, then divide rest for EQ bands
    auto volArea = area.removeFromRight(50).reduced(2);
    volumeLabel->setBounds(volArea.removeFromBottom(20));
    volumeSlider->setBounds(volArea);

    auto sliderWidth = area.getWidth() / 10;
    for (int i = 0; i < 10; ++i)
    {
        auto sliderArea = area.removeFromLeft(sliderWidth).reduced(2);
        bandLabels[i]->setBounds(sliderArea.removeFromBottom(20));
        bandSliders[i]->setBounds(sliderArea);
    }
}
