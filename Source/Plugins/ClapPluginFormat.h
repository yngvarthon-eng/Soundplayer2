#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * JUCE AudioPluginFormat that hosts CLAP plugins.
 *
 * Discovered by scanning for .clap files. Each .clap can contain multiple
 * plugin descriptors (sub-plugins). On Linux/Mac this uses dlopen; on
 * Windows it uses LoadLibrary.
 */
class ClapPluginFormat : public juce::AudioPluginFormat
{
public:
    ClapPluginFormat();
    ~ClapPluginFormat() override;

    // AudioPluginFormat interface
    juce::String getName() const override { return "CLAP"; }
    void findAllTypesForFile(juce::OwnedArray<juce::PluginDescription>& results,
                             const juce::String& fileOrIdentifier) override;
    bool fileMightContainThisPluginType(const juce::String& fileOrIdentifier) override;
    juce::String getNameOfPluginFromIdentifier(const juce::String& fileOrIdentifier) override;
    bool pluginNeedsRescanning(const juce::PluginDescription& description) override;
    bool doesPluginStillExist(const juce::PluginDescription& description) override;
    bool canScanForPlugins() const override { return true; }
    bool isTrivialToScan() const override { return false; }
    juce::FileSearchPath getDefaultLocationsToSearch() override;
    bool requiresUnblockedMessageThreadDuringCreation(const juce::PluginDescription&) const override { return false; }
    juce::StringArray searchPathsForPlugins(const juce::FileSearchPath& directoriesToSearch,
                                            bool recursive,
                                            bool allowPluginsWhichRequireAsynchronousInstantiation) override;

protected:
    void createPluginInstance(const juce::PluginDescription& desc,
                              double initialSampleRate,
                              int initialBufferSize,
                              PluginCreationCallback callback) override;
};
