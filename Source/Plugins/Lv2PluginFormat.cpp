#ifdef SOUNDPLAYER_LV2_SUPPORT

#include "Lv2PluginFormat.h"

#include <lilv/lilv.h>

// ---------------------------------------------------------------------------
// LV2 URIs used for port type detection
// ---------------------------------------------------------------------------
static const char* LV2_AUDIO_PORT_URI  = "http://lv2plug.in/ns/lv2core#AudioPort";
static const char* LV2_CONTROL_PORT_URI= "http://lv2plug.in/ns/lv2core#ControlPort";
static const char* LV2_INPUT_PORT_URI  = "http://lv2plug.in/ns/lv2core#InputPort";
static const char* LV2_OUTPUT_PORT_URI = "http://lv2plug.in/ns/lv2core#OutputPort";

// ---------------------------------------------------------------------------
// Pimpl — owns the LilvWorld
// ---------------------------------------------------------------------------
struct Lv2PluginFormat::Pimpl
{
    LilvWorld* world = nullptr;

    Pimpl()
    {
        world = lilv_world_new();
        lilv_world_load_all(world);
    }

    ~Pimpl()
    {
        lilv_world_free(world);
    }
};

// ---------------------------------------------------------------------------
// LV2 plugin instance
// ---------------------------------------------------------------------------
class Lv2PluginInstance : public juce::AudioPluginInstance
{
public:
    Lv2PluginInstance(LilvWorld* world,
                      const LilvPlugin* plugin,
                      const juce::PluginDescription& desc,
                      double sampleRate,
                      int blockSize)
        : lilvWorld(world), lilvPlugin(plugin), pluginDescription(desc)
    {
        setPlayConfigDetails(2, 2, sampleRate, blockSize);
        buildPortMap();
        instantiate(sampleRate, blockSize);
    }

    ~Lv2PluginInstance() override
    {
        if (handle != nullptr)
        {
            lilv_instance_deactivate(handle);
            lilv_instance_free(handle);
        }
    }

    void fillInPluginDescription(juce::PluginDescription& d) const override { d = pluginDescription; }
    const juce::String getName() const override { return pluginDescription.name; }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override
    {
        if (handle != nullptr)
        {
            lilv_instance_deactivate(handle);
            lilv_instance_free(handle);
            handle = nullptr;
        }
        setPlayConfigDetails(2, 2, sampleRate, maximumExpectedSamplesPerBlock);
        instantiate(sampleRate, maximumExpectedSamplesPerBlock);
    }

    void releaseResources() override
    {
        if (handle != nullptr)
        {
            lilv_instance_deactivate(handle);
            lilv_instance_free(handle);
            handle = nullptr;
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (handle == nullptr)
            return;

        const int numSamples = buffer.getNumSamples();

        // Connect audio ports
        int audioInCount = 0, audioOutCount = 0;
        const uint32_t numPorts = lilv_plugin_get_num_ports(lilvPlugin);
        for (uint32_t i = 0; i < numPorts; ++i)
        {
            const LilvPort* port = lilv_plugin_get_port_by_index(lilvPlugin, i);
            if (isAudioPort(port))
            {
                if (isInputPort(port))
                {
                    int ch = juce::jmin(audioInCount, buffer.getNumChannels() - 1);
                    lilv_instance_connect_port(handle, i,
                        ch >= 0 ? buffer.getWritePointer(ch) : scratchBuf.data());
                    ++audioInCount;
                }
                else
                {
                    int ch = juce::jmin(audioOutCount, buffer.getNumChannels() - 1);
                    lilv_instance_connect_port(handle, i,
                        ch >= 0 ? buffer.getWritePointer(ch) : scratchBuf.data());
                    ++audioOutCount;
                }
            }
        }

        lilv_instance_run(handle, (uint32_t)numSamples);
    }

    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    LilvWorld*           lilvWorld;
    const LilvPlugin*    lilvPlugin;
    LilvInstance*        handle = nullptr;
    juce::PluginDescription pluginDescription;
    std::vector<float>   controlValues;
    std::vector<float>   scratchBuf;

    void buildPortMap()
    {
        const uint32_t numPorts = lilv_plugin_get_num_ports(lilvPlugin);
        controlValues.resize(numPorts, 0.0f);
        scratchBuf.resize(8192, 0.0f);

        // Fill default control values from port metadata
        lilv_plugin_get_port_ranges_float(lilvPlugin, nullptr, nullptr, controlValues.data());
    }

    void instantiate(double sampleRate, int blockSize)
    {
        scratchBuf.resize((size_t)juce::jmax(8192, blockSize * 2), 0.0f);
        handle = lilv_plugin_instantiate(lilvPlugin, sampleRate, nullptr);
        if (handle == nullptr)
            return;

        // Connect control ports to our buffer
        const uint32_t numPorts = lilv_plugin_get_num_ports(lilvPlugin);
        for (uint32_t i = 0; i < numPorts; ++i)
        {
            const LilvPort* port = lilv_plugin_get_port_by_index(lilvPlugin, i);
            if (isControlPort(port))
                lilv_instance_connect_port(handle, i, &controlValues[i]);
        }
        lilv_instance_activate(handle);
    }

    bool isAudioPort(const LilvPort* port) const
    {
        LilvNode* audioClass = lilv_new_uri(lilvWorld, LV2_AUDIO_PORT_URI);
        bool result = lilv_port_is_a(lilvPlugin, port, audioClass);
        lilv_node_free(audioClass);
        return result;
    }

    bool isControlPort(const LilvPort* port) const
    {
        LilvNode* ctrlClass = lilv_new_uri(lilvWorld, LV2_CONTROL_PORT_URI);
        bool result = lilv_port_is_a(lilvPlugin, port, ctrlClass);
        lilv_node_free(ctrlClass);
        return result;
    }

    bool isInputPort(const LilvPort* port) const
    {
        LilvNode* inClass = lilv_new_uri(lilvWorld, LV2_INPUT_PORT_URI);
        bool result = lilv_port_is_a(lilvPlugin, port, inClass);
        lilv_node_free(inClass);
        return result;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Lv2PluginInstance)
};

// ---------------------------------------------------------------------------
// Lv2PluginFormat
// ---------------------------------------------------------------------------
Lv2PluginFormat::Lv2PluginFormat()
    : pimpl(std::make_unique<Pimpl>())
{
}

Lv2PluginFormat::~Lv2PluginFormat() = default;

bool Lv2PluginFormat::fileMightContainThisPluginType(const juce::String& fileOrIdentifier)
{
    // LV2 bundles are directories ending in .lv2; identifiers can also be URIs
    return juce::File(fileOrIdentifier).isDirectory()
        || fileOrIdentifier.startsWith("http://")
        || fileOrIdentifier.startsWith("https://");
}

juce::String Lv2PluginFormat::getNameOfPluginFromIdentifier(const juce::String& fileOrIdentifier)
{
    return fileOrIdentifier;
}

juce::FileSearchPath Lv2PluginFormat::getDefaultLocationsToSearch()
{
    juce::FileSearchPath paths;
#if JUCE_LINUX
    paths.add(juce::File("/usr/lib/lv2"));
    paths.add(juce::File("/usr/local/lib/lv2"));
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".lv2"));
#elif JUCE_MAC
    paths.add(juce::File("/Library/Audio/Plug-Ins/LV2"));
#elif JUCE_WINDOWS
    paths.add(juce::File("C:\\Program Files\\Common Files\\LV2"));
#endif
    return paths;
}

bool Lv2PluginFormat::pluginNeedsRescanning(const juce::PluginDescription&)
{
    return false;
}

bool Lv2PluginFormat::doesPluginStillExist(const juce::PluginDescription& desc)
{
    LilvNode* uri = lilv_new_uri(pimpl->world, desc.fileOrIdentifier.toRawUTF8());
    const LilvPlugins* plugins = lilv_world_get_all_plugins(pimpl->world);
    const LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, uri);
    lilv_node_free(uri);
    return plugin != nullptr;
}

juce::StringArray Lv2PluginFormat::searchPathsForPlugins(
    const juce::FileSearchPath& dirs,
    bool recursive,
    bool)
{
    juce::StringArray results;
    for (int i = 0; i < dirs.getNumPaths(); ++i)
    {
        auto bundles = dirs[i].findChildFiles(
            juce::File::findDirectories, recursive, "*.lv2");
        for (auto& b : bundles)
            results.add(b.getFullPathName());
    }
    return results;
}

void Lv2PluginFormat::discoverAllPlugins(juce::OwnedArray<juce::PluginDescription>& results)
{
    const LilvPlugins* plugins = lilv_world_get_all_plugins(pimpl->world);
    LILV_FOREACH(plugins, it, plugins)
    {
        const LilvPlugin* p = lilv_plugins_get(plugins, it);
        const LilvNode* uriNode = lilv_plugin_get_uri(p);

        LilvNode* nameNode   = lilv_plugin_get_name(p);
        LilvNode* authorNode = lilv_plugin_get_author_name(p);

        auto* pd = new juce::PluginDescription();
        pd->name                = nameNode  ? lilv_node_as_string(nameNode)  : lilv_node_as_string(uriNode);
        pd->descriptiveName     = pd->name;
        pd->pluginFormatName    = "LV2";
        pd->manufacturerName    = authorNode ? lilv_node_as_string(authorNode) : "";
        pd->fileOrIdentifier    = lilv_node_as_string(uriNode);
        pd->uniqueId            = pd->fileOrIdentifier.hashCode();
        pd->lastFileModTime     = juce::Time::getCurrentTime();
        pd->lastInfoUpdateTime  = juce::Time::getCurrentTime();
        pd->numInputChannels    = 2;
        pd->numOutputChannels   = 2;
        pd->isInstrument        = false;

        lilv_node_free(nameNode);
        lilv_node_free(authorNode);

        results.add(pd);
    }
}

void Lv2PluginFormat::findAllTypesForFile(
    juce::OwnedArray<juce::PluginDescription>& results,
    const juce::String& fileOrIdentifier)
{
    // fileOrIdentifier is either a bundle path or a plugin URI
    if (fileOrIdentifier.startsWith("http://") || fileOrIdentifier.startsWith("https://"))
    {
        LilvNode* uri = lilv_new_uri(pimpl->world, fileOrIdentifier.toRawUTF8());
        const LilvPlugins* all = lilv_world_get_all_plugins(pimpl->world);
        const LilvPlugin* p = lilv_plugins_get_by_uri(all, uri);
        lilv_node_free(uri);

        if (p == nullptr)
            return;

        LilvNode* nameNode   = lilv_plugin_get_name(p);
        LilvNode* authorNode = lilv_plugin_get_author_name(p);

        auto* pd = new juce::PluginDescription();
        pd->name             = nameNode  ? lilv_node_as_string(nameNode)  : fileOrIdentifier;
        pd->descriptiveName  = pd->name;
        pd->pluginFormatName = "LV2";
        pd->manufacturerName = authorNode ? lilv_node_as_string(authorNode) : "";
        pd->fileOrIdentifier = fileOrIdentifier;
        pd->uniqueId         = fileOrIdentifier.hashCode();
        pd->lastInfoUpdateTime = juce::Time::getCurrentTime();

        lilv_node_free(nameNode);
        lilv_node_free(authorNode);
        results.add(pd);
    }
}

void Lv2PluginFormat::createPluginInstance(
    const juce::PluginDescription& desc,
    double initialSampleRate,
    int initialBufferSize,
    PluginCreationCallback callback)
{
    LilvNode* uri = lilv_new_uri(pimpl->world, desc.fileOrIdentifier.toRawUTF8());
    const LilvPlugins* all = lilv_world_get_all_plugins(pimpl->world);
    const LilvPlugin* p = lilv_plugins_get_by_uri(all, uri);
    lilv_node_free(uri);

    if (p == nullptr)
    {
        callback(nullptr, "LV2 plugin URI not found: " + desc.fileOrIdentifier);
        return;
    }

    auto instance = std::make_unique<Lv2PluginInstance>(
        pimpl->world, p, desc, initialSampleRate, initialBufferSize);

    callback(std::move(instance), {});
}

#endif // SOUNDPLAYER_LV2_SUPPORT
