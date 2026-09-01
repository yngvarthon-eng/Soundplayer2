#include "EqPresetManager.h"

EqPresetManager::EqPresetManager()
{
    addBuiltInPresets();
    loadFromDisk();
}

void EqPresetManager::addBuiltInPresets()
{
    auto add = [&](const char* name, std::array<float, 10> gains) {
        presets.push_back({ name, gains, true });
    };

    add("Flat",         {  0.f,  0.f,  0.f,  0.f,  0.f,  0.f,  0.f,  0.f,  0.f,  0.f });
    add("Bass Boost",   {  6.f,  5.f,  4.f,  2.f,  0.f,  0.f,  0.f,  0.f,  0.f,  0.f });
    add("Treble Boost", {  0.f,  0.f,  0.f,  0.f,  0.f,  0.f,  2.f,  4.f,  5.f,  6.f });
    add("Rock",         {  5.f,  3.f,  1.f, -1.f, -2.f,  0.f,  2.f,  4.f,  5.f,  5.f });
    add("Classical",    {  0.f,  0.f,  0.f,  0.f,  0.f,  0.f, -2.f, -3.f, -3.f, -4.f });
    add("Pop",          { -2.f, -1.f,  0.f,  2.f,  4.f,  4.f,  2.f,  0.f, -1.f, -2.f });
    add("Jazz",         {  3.f,  2.f,  0.f,  2.f, -2.f, -2.f,  0.f,  1.f,  3.f,  3.f });
}

void EqPresetManager::savePreset(const juce::String& name, const std::array<float, 10>& gains)
{
    for (auto& p : presets)
    {
        if (!p.isBuiltIn && p.name == name)
        {
            p.gains = gains;
            saveToDisk();
            return;
        }
    }
    presets.push_back({ name, gains, false });
    saveToDisk();
}

bool EqPresetManager::deletePreset(const juce::String& name)
{
    for (auto it = presets.begin(); it != presets.end(); ++it)
    {
        if (!it->isBuiltIn && it->name == name)
        {
            presets.erase(it);
            saveToDisk();
            return true;
        }
    }
    return false;
}

juce::File EqPresetManager::getPresetsFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("SoundPlayer2")
               .getChildFile("eq_presets.xml");
}

void EqPresetManager::loadFromDisk()
{
    auto file = getPresetsFile();
    if (!file.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse(file);
    if (!xml || xml->getTagName() != "EqPresets")
        return;

    for (auto* child : xml->getChildIterator())
    {
        if (child->getTagName() != "Preset")
            continue;

        EqPreset preset;
        preset.name = child->getStringAttribute("name");
        preset.isBuiltIn = false;

        if (preset.name.isEmpty())
            continue;

        bool isDuplicate = false;
        for (const auto& p : presets)
            if (p.isBuiltIn && p.name == preset.name)
                isDuplicate = true;
        if (isDuplicate)
            continue;

        for (int i = 0; i < 10; ++i)
            preset.gains[i] = (float)child->getDoubleAttribute("band" + juce::String(i), 0.0);

        presets.push_back(preset);
    }
}

void EqPresetManager::saveToDisk() const
{
    auto file = getPresetsFile();
    file.getParentDirectory().createDirectory();

    juce::XmlElement root("EqPresets");

    for (const auto& preset : presets)
    {
        if (preset.isBuiltIn)
            continue;

        auto* child = root.createNewChildElement("Preset");
        child->setAttribute("name", preset.name);
        for (int i = 0; i < 10; ++i)
            child->setAttribute("band" + juce::String(i), (double)preset.gains[i]);
    }

    root.writeTo(file);
}
