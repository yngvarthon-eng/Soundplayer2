#pragma once

#ifdef SOUNDPLAYER_LV2_SUPPORT

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * JUCE AudioPluginFormat that hosts LV2 plugins via lilv.
 *
 * Only compiled when SOUNDPLAYER_LV2_SUPPORT is defined (requires lilv-0).
 * Scans installed LV2 bundles using lilv_world_load_all().
 */
class Lv2PluginFormat : public juce::AudioPluginFormat
{
public:
    Lv2PluginFormat();
    ~Lv2PluginFormat() override;

    juce::String getName() const override { return "LV2"; }

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

    juce::StringArray searchPathsForPlugins(
        const juce::FileSearchPath& directoriesToSearch,
        bool recursive,
        bool allowPluginsWhichRequireAsynchronousInstantiation) override;

    /** Populate the known-plugin list with all installed LV2 plugins. */
    void discoverAllPlugins(juce::OwnedArray<juce::PluginDescription>& results);

protected:
    void createPluginInstance(const juce::PluginDescription& desc,
                              double initialSampleRate,
                              int initialBufferSize,
                              PluginCreationCallback callback) override;

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Lv2PluginFormat)
};

#endif // SOUNDPLAYER_LV2_SUPPORT
