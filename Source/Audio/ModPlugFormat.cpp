#if defined(SOUNDPLAYER_OPENMPT_SUPPORT) || defined(SOUNDPLAYER_XMP_SUPPORT)

#include "ModPlugFormat.h"

#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
#include <libopenmpt/libopenmpt.hpp>
#endif

#ifdef SOUNDPLAYER_XMP_SUPPORT
#include <xmp.h>
#endif

#include <cstdint>
#include <cstdio>

#ifdef SOUNDPLAYER_OPENMPT_SUPPORT

// ---------------------------------------------------------------------------
// OpenMptAudioFormatReader
// ---------------------------------------------------------------------------

OpenMptAudioFormatReader::OpenMptAudioFormatReader(juce::InputStream* stream,
                                                   const juce::String& formatName)
    : juce::AudioFormatReader(nullptr, formatName) // nullptr: we manage stream lifetime below
{
    sampleRate            = kSampleRate;
    numChannels           = (unsigned int)kNumChannels;
    bitsPerSample         = 32;
    usesFloatingPointData = true;

    // Read enough bytes for libopenmpt's header probe. This avoids consuming
    // infinite streams (e.g. internet radio) before we know it's a module file.
    std::size_t probeSize = openmpt::probe_file_header_get_recommended_size();
    std::vector<char> header(probeSize);
    int headerRead = stream->read(header.data(), (int)probeSize);
    header.resize((size_t)headerRead);

    auto probeResult = openmpt::probe_file_header(
        0x1ull | 0x2ull, // default flags (avoid deprecated constant)
        reinterpret_cast<const std::uint8_t*>(header.data()), header.size(),
        static_cast<std::uint64_t>(-1)); // -1 = unknown file size

    if (probeResult != openmpt::probe_file_header_result_success)
        return; // leave lengthInSamples = 0; createReaderFor() will discard us

    // Prepend the already-read header bytes, then stream the remainder.
    std::vector<char> fileData(header.begin(), header.end());
    {
        juce::MemoryOutputStream mo;
        mo.writeFromInputStream(*stream, -1);
        const char* rest = static_cast<const char*>(mo.getData());
        fileData.insert(fileData.end(), rest, rest + mo.getDataSize());
    }

    try
    {
        mod = std::make_unique<openmpt::module>(fileData, std::cerr);
    }
    catch (const openmpt::exception&)
    {
        return; // unrecognised or corrupt file
    }

    // Sinc8 interpolation — highest quality resampling
    mod->set_render_param(openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, 8);
    // Play once, no infinite looping
    mod->set_repeat_count(0);

    double durationSecs = mod->get_duration_seconds();
    if (durationSecs <= 0.0)
    {
        mod.reset();
        return;
    }
    lengthInSamples = static_cast<juce::int64>(durationSecs * kSampleRate + 0.5);
}

OpenMptAudioFormatReader::~OpenMptAudioFormatReader() = default;

bool OpenMptAudioFormatReader::readSamples(int* const* destSamples,
                                           int numDestChannels,
                                           int startOffsetInDestBuffer,
                                           juce::int64 startSampleInFile,
                                           int numSamples)
{
    if (mod == nullptr)
    {
        for (int ch = 0; ch < numDestChannels; ++ch)
            if (destSamples[ch] != nullptr)
                juce::zeromem(destSamples[ch] + startOffsetInDestBuffer,
                              (size_t)numSamples * sizeof(float));
        return false;
    }

    // Seek if playback position jumped (e.g. scrubbing)
    if (startSampleInFile != currentSample)
    {
        double seekSecs = startSampleInFile / (double)kSampleRate;
        mod->set_position_seconds(seekSecs);
        currentSample = startSampleInFile;
    }

    // Decode into separate left/right float buffers
    decodeLeft.resize((size_t)numSamples);
    decodeRight.resize((size_t)numSamples);

    std::size_t framesDecoded = mod->read(kSampleRate, (std::size_t)numSamples,
                                          decodeLeft.data(), decodeRight.data());

    // Zero the tail if end of module was reached
    if ((int)framesDecoded < numSamples)
    {
        std::fill(decodeLeft.begin()  + (std::ptrdiff_t)framesDecoded, decodeLeft.end(),  0.0f);
        std::fill(decodeRight.begin() + (std::ptrdiff_t)framesDecoded, decodeRight.end(), 0.0f);
    }

    // usesFloatingPointData == true: JUCE treats int* dest as float*
    const float* channels[2] = { decodeLeft.data(), decodeRight.data() };
    for (int ch = 0; ch < numDestChannels; ++ch)
    {
        if (destSamples[ch] == nullptr) continue;
        float* dst = reinterpret_cast<float*>(destSamples[ch]) + startOffsetInDestBuffer;
        int srcCh  = juce::jmin(ch, kNumChannels - 1); // clamp to stereo
        std::memcpy(dst, channels[srcCh], (size_t)numSamples * sizeof(float));
    }

    currentSample += (juce::int64)framesDecoded;
    return true;
}

int OpenMptAudioFormatReader::getCurrentRow() const
{
    return mod ? mod->get_current_row() : -1;
}

int OpenMptAudioFormatReader::getCurrentPattern() const
{
    return mod ? mod->get_current_pattern() : -1;
}

#endif // SOUNDPLAYER_OPENMPT_SUPPORT

#ifdef SOUNDPLAYER_XMP_SUPPORT

class XmpAudioFormatReader : public juce::AudioFormatReader
{
public:
    XmpAudioFormatReader(const void* moduleData, size_t moduleSize, const juce::String& formatName)
        : juce::AudioFormatReader(nullptr, formatName)
    {
        sampleRate            = kSampleRate;
        numChannels           = (unsigned int)kNumChannels;
        bitsPerSample         = 32;
        usesFloatingPointData = true;

        ctx = xmp_create_context();
        if (ctx == nullptr)
            return;

        if (xmp_load_module_from_memory(ctx, moduleData, (long)moduleSize) != 0)
            return;
        moduleLoaded = true;

        if (xmp_start_player(ctx, kSampleRate, 0) != 0)
            return;
        playerStarted = true;

        xmp_frame_info info{};
        xmp_get_frame_info(ctx, &info);
        if (info.total_time <= 0)
            return;

        lengthInSamples = static_cast<juce::int64>((static_cast<int64_t>(info.total_time) * kSampleRate) / 1000);
    }

    ~XmpAudioFormatReader() override
    {
        if (ctx == nullptr)
            return;

        if (playerStarted)
            xmp_end_player(ctx);
        if (moduleLoaded)
            xmp_release_module(ctx);

        xmp_free_context(ctx);
    }

    bool readSamples(int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                     juce::int64 startSampleInFile, int numSamples) override
    {
        if (ctx == nullptr || !playerStarted)
        {
            for (int ch = 0; ch < numDestChannels; ++ch)
                if (destSamples[ch] != nullptr)
                    juce::zeromem(destSamples[ch] + startOffsetInDestBuffer,
                                  (size_t)numSamples * sizeof(float));
            return false;
        }

        if (startSampleInFile != currentSample)
        {
            const int seekMs = static_cast<int>((startSampleInFile * 1000) / kSampleRate);
            if (xmp_seek_time(ctx, seekMs) < 0)
            {
                for (int ch = 0; ch < numDestChannels; ++ch)
                    if (destSamples[ch] != nullptr)
                        juce::zeromem(destSamples[ch] + startOffsetInDestBuffer,
                                      (size_t)numSamples * sizeof(float));
                return false;
            }
            currentSample = startSampleInFile;
        }

        interleavedPcm.resize((size_t)numSamples * (size_t)kNumChannels);
        decodeLeft.resize((size_t)numSamples);
        decodeRight.resize((size_t)numSamples);

        const int result = xmp_play_buffer(ctx,
                                           interleavedPcm.data(),
                                           (int)(interleavedPcm.size() * sizeof(int16_t)),
                                           0);

        constexpr float kInt16ToFloat = 1.0f / 32768.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            decodeLeft[(size_t)i] = (float)interleavedPcm[(size_t)i * 2] * kInt16ToFloat;
            decodeRight[(size_t)i] = (float)interleavedPcm[(size_t)i * 2 + 1] * kInt16ToFloat;
        }

        const float* channels[2] = { decodeLeft.data(), decodeRight.data() };
        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            if (destSamples[ch] == nullptr)
                continue;

            float* dst = reinterpret_cast<float*>(destSamples[ch]) + startOffsetInDestBuffer;
            const int srcCh = juce::jmin(ch, kNumChannels - 1);
            std::memcpy(dst, channels[srcCh], (size_t)numSamples * sizeof(float));
        }

        currentSample += numSamples;
        return result == 0;
    }

private:
    xmp_context ctx = nullptr;
    bool moduleLoaded = false;
    bool playerStarted = false;
    juce::int64 currentSample = 0;

    std::vector<int16_t> interleavedPcm;
    std::vector<float> decodeLeft, decodeRight;

    static constexpr int kSampleRate  = 48000;
    static constexpr int kNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XmpAudioFormatReader)
};

#endif // SOUNDPLAYER_XMP_SUPPORT

// ---------------------------------------------------------------------------
// OpenMptAudioFormat
// ---------------------------------------------------------------------------

static const char* const modExtensions[] = {
    "mod", "xm", "it", "s3m", "mptm", "umx",
    "669", "ahx", "amf", "ams", "dbm", "dcm", "digi", "dlm", "dmf", "drm", "dsm",
    "far", "mdl", "med", "mt2", "mtm", "oct", "okt", "umf",
    "ptm", "stm", "ult", "j2b", "gdm", "u2b",
    "mo3", "oxm", "pcm", "cdm", "hvl", "c67", "sfx", "imf", "psm", "stp", "symmod",
    nullptr
};

OpenMptAudioFormat::OpenMptAudioFormat()
    : juce::AudioFormat("Module Tracker", [] {
          juce::StringArray ext;
          for (int i = 0; modExtensions[i] != nullptr; ++i)
              ext.add(modExtensions[i]);
          return ext;
      }())
{
}

juce::Array<int> OpenMptAudioFormat::getPossibleSampleRates()
{
    return { 44100, 48000 };
}

juce::Array<int> OpenMptAudioFormat::getPossibleBitDepths()
{
    return { 32 };
}

juce::AudioFormatReader* OpenMptAudioFormat::createReaderFor(juce::InputStream* sourceStream,
                                                              bool deleteStreamIfOpeningFails)
{
    if (sourceStream == nullptr)
        return nullptr;

    juce::String sourceLabel = "stream input";
    if (auto* fileStream = dynamic_cast<juce::FileInputStream*>(sourceStream))
        sourceLabel = fileStream->getFile().getFileName();

    juce::MemoryBlock moduleData;
    sourceStream->readIntoMemoryBlock(moduleData, -1);

    if (moduleData.getSize() == 0)
    {
        if (deleteStreamIfOpeningFails)
            delete sourceStream;
        return nullptr;
    }

#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    {
        juce::MemoryInputStream openMptStream(moduleData.getData(), moduleData.getSize(), false);
        auto* openMptReader = new OpenMptAudioFormatReader(&openMptStream, getFormatName());
        if (openMptReader->lengthInSamples > 0)
        {
            std::fprintf(stderr, "Module decoder: using OpenMPT for '%s'\n", sourceLabel.toRawUTF8());
            if (deleteStreamIfOpeningFails)
                delete sourceStream;
            return openMptReader;
        }

        std::fprintf(stderr, "Module decoder: OpenMPT failed for '%s', trying libxmp fallback\n", sourceLabel.toRawUTF8());
        delete openMptReader;
    }
#endif

#ifdef SOUNDPLAYER_XMP_SUPPORT
    {
        auto* xmpReader = new XmpAudioFormatReader(moduleData.getData(), moduleData.getSize(), getFormatName());
        if (xmpReader->lengthInSamples > 0)
        {
            std::fprintf(stderr, "Module decoder: using libxmp fallback for '%s'\n", sourceLabel.toRawUTF8());
            if (deleteStreamIfOpeningFails)
                delete sourceStream;
            return xmpReader;
        }

        delete xmpReader;
    }
#endif

    if (deleteStreamIfOpeningFails)
        delete sourceStream;
    return nullptr;
}

#endif // SOUNDPLAYER_OPENMPT_SUPPORT || SOUNDPLAYER_XMP_SUPPORT
