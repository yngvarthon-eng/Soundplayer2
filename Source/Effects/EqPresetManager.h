#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>

struct EqPreset
{
    juce::String name;
    std::array<float, 10> gains;
    bool isBuiltIn = false;
};

class EqPresetManager
{
public:
    EqPresetManager();

    const std::vector<EqPreset>& getPresets() const { return presets; }

    void savePreset(const juce::String& name, const std::array<float, 10>& gains);
    bool deletePreset(const juce::String& name); // returns false if built-in or not found

    void loadFromDisk();
    void saveToDisk() const;

private:
    std::vector<EqPreset> presets;

    juce::File getPresetsFile() const;
    void addBuiltInPresets();
};
