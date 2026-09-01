#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

/**
 * Manages plugin format registration, scanning, and instantiation.
 *
 * Registers VST3, CLAP (via ClapPluginFormat), and optionally LV2.
 * Maintains a KnownPluginList that survives between sessions via XML.
 */
class PluginHostManager
{
public:
    PluginHostManager();
    ~PluginHostManager() = default;

    /** Register all supported formats. Must be called once before use. */
    void initialise();

    /**
     * Scan a directory for plugins, calling progressCallback(found, total)
     * periodically. Blocking — call from a background thread or a
     * ThreadWithProgressWindow.
     */
    void scanDirectory(const juce::File& directory,
                       std::function<void(int, int)> progressCallback = nullptr);

    /** Non-blocking: scan the default platform plugin directories. */
    void scanDefaultDirectories(std::function<void(int, int)> progressCallback = nullptr);

    const juce::KnownPluginList& getKnownPlugins() const { return knownPlugins; }
    juce::KnownPluginList& getKnownPlugins() { return knownPlugins; }

    juce::AudioPluginFormatManager& getFormatManager() { return formatManager; }

    /**
     * Instantiate a plugin from a description.
     * Returns nullptr on failure; errorMessage is set.
     */
    std::unique_ptr<juce::AudioPluginInstance> loadPlugin(
        const juce::PluginDescription& desc,
        double sampleRate,
        int blockSize,
        juce::String& errorMessage);

    void saveState(const juce::File& xmlFile) const;
    bool loadState(const juce::File& xmlFile);

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHostManager)
};
