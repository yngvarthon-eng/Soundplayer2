#include "PluginHostManager.h"
#include "ClapPluginFormat.h"

#ifdef SOUNDPLAYER_LV2_SUPPORT
#include "Lv2PluginFormat.h"
#endif

PluginHostManager::PluginHostManager() = default;

void PluginHostManager::initialise()
{
    formatManager.addDefaultFormats(); // adds VST3 (when JUCE_PLUGINHOST_VST3=1)
    formatManager.addFormat(new ClapPluginFormat());

#ifdef SOUNDPLAYER_LV2_SUPPORT
    formatManager.addFormat(new Lv2PluginFormat());
#endif
}

void PluginHostManager::scanDirectory(const juce::File& directory,
                                      std::function<void(int, int)> progressCallback)
{
    juce::FileSearchPath searchPath;
    searchPath.add(directory);
    int found = 0;

    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        juce::PluginDirectoryScanner scanner(
            knownPlugins,
            *formatManager.getFormat(i),
            searchPath,
            true,       // recursive
            juce::File{});

        juce::String currentPlugin;
        while (scanner.scanNextFile(true, currentPlugin))
        {
            ++found;
            if (progressCallback)
                progressCallback(found, found);
        }
    }
}

void PluginHostManager::scanDefaultDirectories(
    std::function<void(int, int)> progressCallback)
{
    int found = 0;

    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* fmt = formatManager.getFormat(i);
        juce::FileSearchPath searchPath = fmt->getDefaultLocationsToSearch();

        juce::PluginDirectoryScanner scanner(
            knownPlugins,
            *fmt,
            searchPath,
            true,
            juce::File{});

        juce::String currentPlugin;
        while (scanner.scanNextFile(true, currentPlugin))
        {
            ++found;
            if (progressCallback)
                progressCallback(found, found);
        }
    }
}

std::unique_ptr<juce::AudioPluginInstance> PluginHostManager::loadPlugin(
    const juce::PluginDescription& desc,
    double sampleRate,
    int blockSize,
    juce::String& errorMessage)
{
    return formatManager.createPluginInstance(desc, sampleRate, blockSize, errorMessage);
}

void PluginHostManager::saveState(const juce::File& xmlFile) const
{
    auto xml = knownPlugins.createXml();
    if (xml != nullptr)
        xml->writeTo(xmlFile);
}

bool PluginHostManager::loadState(const juce::File& xmlFile)
{
    if (!xmlFile.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(xmlFile);
    if (xml == nullptr)
        return false;

    knownPlugins.recreateFromXml(*xml);
    return true;
}
