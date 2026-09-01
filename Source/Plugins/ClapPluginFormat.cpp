#include "ClapPluginFormat.h"

#include <clap/clap.h>

#if JUCE_WINDOWS
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  using LibHandle = HMODULE;
  static LibHandle openLib(const char* path)  { return LoadLibraryA(path); }
  static void*     symLib(LibHandle h, const char* s) { return (void*)GetProcAddress(h, s); }
  static void      closeLib(LibHandle h)      { FreeLibrary(h); }
#else
  #include <dlfcn.h>
  using LibHandle = void*;
  static LibHandle openLib(const char* path)  { return dlopen(path, RTLD_LAZY | RTLD_LOCAL); }
  static void*     symLib(LibHandle h, const char* s) { return dlsym(h, s); }
  static void      closeLib(LibHandle h)      { dlclose(h); }
#endif

// ---------------------------------------------------------------------------
// Minimal CLAP host callbacks (required by the CLAP spec)
// ---------------------------------------------------------------------------
static const void* clapHostGetExtension(const clap_host_t*, const char*) { return nullptr; }
static void        clapHostRequestRestart(const clap_host_t*)             {}
static void        clapHostRequestProcess(const clap_host_t*)             {}
static void        clapHostRequestCallback(const clap_host_t*)            {}

static const clap_host_t g_clapHost = {
    CLAP_VERSION,
    nullptr,                   // host_data
    "SoundPlayer2",            // name
    "SoundPlayer",             // vendor
    "",                        // url
    "0.1.0",                   // version
    clapHostGetExtension,
    clapHostRequestRestart,
    clapHostRequestProcess,
    clapHostRequestCallback
};

// ---------------------------------------------------------------------------
// ClapPluginInstance
// ---------------------------------------------------------------------------
class ClapPluginInstance : public juce::AudioPluginInstance
{
public:
    ClapPluginInstance(LibHandle libHandle,
                       const clap_plugin_t* plugin,
                       const juce::PluginDescription& desc)
        : lib(libHandle), clapPlugin(plugin), pluginDescription(desc)
    {
        setPlayConfigDetails(2, 2, 44100.0, 512);
    }

    ~ClapPluginInstance() override
    {
        if (activated)
        {
            clapPlugin->stop_processing(clapPlugin);
            clapPlugin->deactivate(clapPlugin);
        }
        clapPlugin->destroy(clapPlugin);
        closeLib(lib);
    }

    // AudioPluginInstance
    void fillInPluginDescription(juce::PluginDescription& d) const override { d = pluginDescription; }

    // AudioProcessor
    const juce::String getName() const override { return pluginDescription.name; }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override
    {
        if (activated)
        {
            clapPlugin->stop_processing(clapPlugin);
            clapPlugin->deactivate(clapPlugin);
        }
        activated = clapPlugin->activate(clapPlugin, sampleRate, 1,
                                          (uint32_t)maximumExpectedSamplesPerBlock);
        if (activated)
            clapPlugin->start_processing(clapPlugin);

        setPlayConfigDetails(2, 2, sampleRate, maximumExpectedSamplesPerBlock);
        scratchLeft.resize((size_t)maximumExpectedSamplesPerBlock, 0.0f);
        scratchRight.resize((size_t)maximumExpectedSamplesPerBlock, 0.0f);
    }

    void releaseResources() override
    {
        if (activated)
        {
            clapPlugin->stop_processing(clapPlugin);
            clapPlugin->deactivate(clapPlugin);
            activated = false;
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (!activated)
            return;

        const int numSamples = buffer.getNumSamples();

        // Build CLAP audio buffers (stereo in/out)
        const float* inPtrs[2]  = {
            buffer.getNumChannels() > 0 ? buffer.getReadPointer(0) : scratchLeft.data(),
            buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : scratchRight.data()
        };
        float* outPtrs[2] = {
            buffer.getNumChannels() > 0 ? buffer.getWritePointer(0) : scratchLeft.data(),
            buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : scratchRight.data()
        };

        clap_audio_buffer_t audioIn  = { const_cast<float**>(inPtrs),  nullptr, 2, 0, 0 };
        clap_audio_buffer_t audioOut = { outPtrs, nullptr, 2, 0, 0 };

        clap_process_t proc{};
        proc.steady_time        = -1;
        proc.frames_count       = (uint32_t)numSamples;
        proc.transport          = nullptr;
        proc.audio_inputs       = &audioIn;
        proc.audio_inputs_count = 1;
        proc.audio_outputs      = &audioOut;
        proc.audio_outputs_count= 1;

        clapPlugin->process(clapPlugin, &proc);
    }

    // AudioProcessor boilerplate
    bool hasEditor() const override
    {
        return clapPlugin->get_extension(clapPlugin, CLAP_EXT_GUI) != nullptr;
    }

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
    LibHandle           lib;
    const clap_plugin_t* clapPlugin;
    juce::PluginDescription pluginDescription;
    bool activated = false;
    std::vector<float> scratchLeft, scratchRight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClapPluginInstance)
};

// ---------------------------------------------------------------------------
// ClapPluginFormat
// ---------------------------------------------------------------------------
ClapPluginFormat::ClapPluginFormat() = default;
ClapPluginFormat::~ClapPluginFormat() = default;

juce::FileSearchPath ClapPluginFormat::getDefaultLocationsToSearch()
{
    juce::FileSearchPath paths;
#if JUCE_LINUX
    paths.add(juce::File("/usr/lib/clap"));
    paths.add(juce::File("/usr/local/lib/clap"));
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".clap"));
#elif JUCE_MAC
    paths.add(juce::File("/Library/Audio/Plug-Ins/CLAP"));
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                         .getChildFile("Library/Audio/Plug-Ins/CLAP"));
#elif JUCE_WINDOWS
    paths.add(juce::File("C:\\Program Files\\Common Files\\CLAP"));
#endif
    return paths;
}

bool ClapPluginFormat::fileMightContainThisPluginType(const juce::String& fileOrIdentifier)
{
    return juce::File(fileOrIdentifier).hasFileExtension(".clap");
}

juce::String ClapPluginFormat::getNameOfPluginFromIdentifier(const juce::String& fileOrIdentifier)
{
    return juce::File(fileOrIdentifier).getFileNameWithoutExtension();
}

bool ClapPluginFormat::pluginNeedsRescanning(const juce::PluginDescription& desc)
{
    return juce::File(desc.fileOrIdentifier).getLastModificationTime()
               > desc.lastFileModTime;
}

bool ClapPluginFormat::doesPluginStillExist(const juce::PluginDescription& desc)
{
    return juce::File(desc.fileOrIdentifier).existsAsFile();
}

juce::StringArray ClapPluginFormat::searchPathsForPlugins(
    const juce::FileSearchPath& dirs,
    bool recursive,
    bool)
{
    juce::StringArray results;
    for (int i = 0; i < dirs.getNumPaths(); ++i)
    {
        auto files = dirs[i].findChildFiles(
            juce::File::findFiles, recursive, "*.clap");
        for (auto& f : files)
            results.add(f.getFullPathName());
    }
    return results;
}

void ClapPluginFormat::findAllTypesForFile(
    juce::OwnedArray<juce::PluginDescription>& results,
    const juce::String& fileOrIdentifier)
{
    juce::File file(fileOrIdentifier);
    if (!file.existsAsFile())
        return;

    auto lib = openLib(file.getFullPathName().toRawUTF8());
    if (lib == nullptr)
        return;

    auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(
        symLib(lib, "clap_entry"));
    if (entry == nullptr || !clap_version_is_compatible(entry->clap_version))
    {
        closeLib(lib);
        return;
    }

    entry->init(file.getFullPathName().toRawUTF8());

    auto* factory = reinterpret_cast<const clap_plugin_factory_t*>(
        entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (factory == nullptr)
    {
        entry->deinit();
        closeLib(lib);
        return;
    }

    const uint32_t count = factory->get_plugin_count(factory);
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto* desc = factory->get_plugin_descriptor(factory, i);
        if (desc == nullptr || !clap_version_is_compatible(desc->clap_version))
            continue;

        auto* pd = new juce::PluginDescription();
        pd->name                = desc->name != nullptr ? desc->name : "";
        pd->descriptiveName     = desc->description != nullptr ? desc->description : pd->name;
        pd->pluginFormatName    = "CLAP";
        pd->manufacturerName    = desc->vendor != nullptr ? desc->vendor : "";
        pd->version             = desc->version != nullptr ? desc->version : "";
        pd->fileOrIdentifier    = file.getFullPathName();
        pd->uniqueId            = juce::String(desc->id).hashCode();
        pd->lastFileModTime     = file.getLastModificationTime();
        pd->lastInfoUpdateTime  = juce::Time::getCurrentTime();
        pd->numInputChannels    = 2;
        pd->numOutputChannels   = 2;
        pd->isInstrument        = false;

        // Stash CLAP plugin ID in deprecatedUid field for retrieval at load time
        pd->deprecatedUid       = juce::String(desc->id).hashCode();

        results.add(pd);
    }

    entry->deinit();
    closeLib(lib);
}

void ClapPluginFormat::createPluginInstance(
    const juce::PluginDescription& desc,
    double initialSampleRate,
    int initialBufferSize,
    PluginCreationCallback callback)
{
    juce::File file(desc.fileOrIdentifier);
    if (!file.existsAsFile())
    {
        callback(nullptr, "File not found: " + desc.fileOrIdentifier);
        return;
    }

    auto lib = openLib(file.getFullPathName().toRawUTF8());
    if (lib == nullptr)
    {
        callback(nullptr, "Failed to open library: " + desc.fileOrIdentifier);
        return;
    }

    auto* entry = reinterpret_cast<const clap_plugin_entry_t*>(
        symLib(lib, "clap_entry"));
    if (entry == nullptr || !clap_version_is_compatible(entry->clap_version))
    {
        closeLib(lib);
        callback(nullptr, "Invalid CLAP entry point");
        return;
    }

    entry->init(file.getFullPathName().toRawUTF8());

    auto* factory = reinterpret_cast<const clap_plugin_factory_t*>(
        entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (factory == nullptr)
    {
        entry->deinit();
        closeLib(lib);
        callback(nullptr, "No CLAP plugin factory found");
        return;
    }

    // Find the descriptor matching our stored uniqueId
    const uint32_t count = factory->get_plugin_count(factory);
    const clap_plugin_t* clapPlugin = nullptr;

    for (uint32_t i = 0; i < count; ++i)
    {
        const auto* d = factory->get_plugin_descriptor(factory, i);
        if (d == nullptr)
            continue;

        if (juce::String(d->id).hashCode() == desc.deprecatedUid)
        {
            clapPlugin = factory->create_plugin(factory, &g_clapHost, d->id);
            break;
        }
    }

    if (clapPlugin == nullptr || !clapPlugin->init(clapPlugin))
    {
        if (clapPlugin != nullptr)
            clapPlugin->destroy(clapPlugin);
        entry->deinit();
        closeLib(lib);
        callback(nullptr, "Failed to create CLAP plugin instance");
        return;
    }

    auto instance = std::make_unique<ClapPluginInstance>(lib, clapPlugin, desc);
    instance->prepareToPlay(initialSampleRate, initialBufferSize);
    callback(std::move(instance), {});
}
