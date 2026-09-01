#include "VideoExporter.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/MultiChannelAudioFormatReaderSource.h"
#include "../Audio/ModPlugFormat.h"
#include "../Xtp/XtpSequencerSource.h"
#include "../Visualization/SpectrumAnalyzer.h"
#include "../Visualization/ModulePatternData.h"
#include "../UI/ModulePatternViewer.h"
#include <cmath>
#include <cstdio>
#include <utility>

#if defined(_WIN32)
#include <stdio.h>
#else
#include <sys/wait.h>
#endif

namespace
{
    juce::String shellQuote(const juce::String& path)
    {
        return "'" + path.replace("'", "'\\''") + "'";
    }

#if defined(_WIN32)
    FILE* openPipe(const juce::String& cmd, const char* mode)  { return _popen(cmd.toRawUTF8(), mode); }
    int   closePipe(FILE* f)                                   { return _pclose(f); }
    bool  exitedCleanly(int status)                            { return status == 0; }
#else
    FILE* openPipe(const juce::String& cmd, const char* mode)  { return popen(cmd.toRawUTF8(), mode); }
    int   closePipe(FILE* f)                                   { return pclose(f); }
    bool  exitedCleanly(int status)                            { return WIFEXITED(status) && WEXITSTATUS(status) == 0; }
#endif

    constexpr int kFps = 25; // matches the live VisualizationComponent timer / plugin decay tuning
    constexpr int kBytesPerPixel = 4; // BGRA

    // Splits the full frame into the (possibly empty) rectangles used for the
    // pattern grid and/or the visualization, per the chosen mode/layout. Applied
    // as a whole-frame compositing step so neither renderer needs to know about
    // the other.
    void computeLayout(VideoExporter::ExportMode mode, VideoExporter::CombinedLayout layout,
                       int width, int height,
                       juce::Rectangle<int>& gridBounds, juce::Rectangle<int>& vizBounds)
    {
        const juce::Rectangle<int> full(0, 0, width, height);
        gridBounds = {};
        vizBounds = {};

        switch (mode)
        {
            case VideoExporter::ExportMode::Visualization:
                vizBounds = full;
                break;
            case VideoExporter::ExportMode::PatternGrid:
                gridBounds = full;
                break;
            case VideoExporter::ExportMode::Combined:
            {
                switch (layout)
                {
                    case VideoExporter::CombinedLayout::SplitGridTop:
                        gridBounds = full.withHeight(height / 2);
                        vizBounds = full.withTrimmedTop(height / 2);
                        break;
                    case VideoExporter::CombinedLayout::SplitVizTop:
                        vizBounds = full.withHeight(height / 2);
                        gridBounds = full.withTrimmedTop(height / 2);
                        break;
                    case VideoExporter::CombinedLayout::PipViz:
                    {
                        gridBounds = full;
                        const int insetW = (int) (width * 0.32);
                        const int insetH = (int) (insetW * height / (double) width);
                        vizBounds = juce::Rectangle<int>(width - insetW - 8, height - insetH - 8, insetW, insetH);
                        break;
                    }
                    case VideoExporter::CombinedLayout::PipGrid:
                    {
                        vizBounds = full;
                        const int insetW = (int) (width * 0.32);
                        const int insetH = (int) (insetW * height / (double) width);
                        gridBounds = juce::Rectangle<int>(width - insetW - 8, height - insetH - 8, insetW, insetH);
                        break;
                    }
                    case VideoExporter::CombinedLayout::Overlay:
                        // Both layers claim the whole frame; the compositing step in run()
                        // draws the visualization over the grid at reduced opacity instead
                        // of full alpha, since both fill their bounds opaquely on their own
                        // (see every VisualizationPlugin::render()/ModulePatternViewer::paint()).
                        gridBounds = full;
                        vizBounds = full;
                        break;
                }
                break;
            }
        }
    }

    // Whole-frame post-process colour treatment. Runs once per frame over the
    // composited image, so it stays uniform across all export modes without
    // touching any plugin's or the pattern grid's own hardcoded colours.
    void applyColorTheme(juce::Image& image, VideoExporter::ColorTheme theme)
    {
        if (theme == VideoExporter::ColorTheme::Classic)
            return;

        juce::Image::BitmapData bitmap(image, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < image.getHeight(); ++y)
        {
            auto* line = bitmap.getLinePointer(y);
            for (int x = 0; x < image.getWidth(); ++x)
            {
                auto* px = line + x * bitmap.pixelStride; // BGRA byte order on little-endian ARGB
                juce::uint8& b = px[0];
                juce::uint8& g = px[1];
                juce::uint8& r = px[2];

                switch (theme)
                {
                    case VideoExporter::ColorTheme::Inverted:
                        r = (juce::uint8) (255 - r);
                        g = (juce::uint8) (255 - g);
                        b = (juce::uint8) (255 - b);
                        break;
                    case VideoExporter::ColorTheme::MonoGreen:
                    case VideoExporter::ColorTheme::MonoAmber:
                    {
                        const float lum = 0.299f * r + 0.587f * g + 0.114f * b;
                        if (theme == VideoExporter::ColorTheme::MonoGreen)
                        {
                            r = (juce::uint8) (lum * 0.25f);
                            g = (juce::uint8) juce::jmin(255.0f, lum * 1.15f);
                            b = (juce::uint8) (lum * 0.25f);
                        }
                        else
                        {
                            r = (juce::uint8) juce::jmin(255.0f, lum * 1.05f);
                            g = (juce::uint8) (lum * 0.72f);
                            b = (juce::uint8) (lum * 0.18f);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    enum class VerticalBand { Top, Center, Bottom };

    // Draws one text layer, if it has any text, at the horizontal position dictated
    // by its slide mode at the given export time. `timeSeconds` is frame-derived
    // (frameIndex / kFps) rather than wall-clock, so the animation is fully
    // deterministic and reproducible frame-for-frame regardless of render speed.
    void drawSlidingText(juce::Image& image, const VideoExporter::TextLayer& layer,
                        VerticalBand band, double timeSeconds)
    {
        if (layer.text.isEmpty())
            return;

        juce::Graphics g(image);
        const int fontHeight = juce::jlimit(12, 72, image.getHeight() / 20);
        g.setFont(juce::Font((float) fontHeight, juce::Font::bold));

        const int textWidth = g.getCurrentFont().getStringWidth(layer.text) + 24;
        const int boxHeight = fontHeight + 16;
        const int margin = 16;
        const int centeredX = (image.getWidth() - textWidth) / 2;

        int y = margin;
        if (band == VerticalBand::Center)
            y = (image.getHeight() - boxHeight) / 2;
        else if (band == VerticalBand::Bottom)
            y = image.getHeight() - boxHeight - margin;

        int x = centeredX;
        switch (layer.slide)
        {
            case VideoExporter::TextSlide::Static:
                break;

            case VideoExporter::TextSlide::Marquee:
            {
                // Travels the full off-screen-right to off-screen-left span, looping
                // forever; marqueeSeconds is the time for one complete crossing.
                const double travel = image.getWidth() + textWidth;
                const double speed = travel / juce::jmax(0.1, layer.marqueeSeconds);
                const double traveled = std::fmod(timeSeconds * speed, travel);
                x = image.getWidth() - (int) traveled;
                break;
            }

            case VideoExporter::TextSlide::TimedSlideInOut:
            {
                // One cycle: slide in, hold centred, slide out, gap -- then repeats.
                const double slideDur = 0.6;
                const double gapDur = 0.8;
                const double cycle = slideDur + layer.holdSeconds + slideDur + gapDur;
                const double t = std::fmod(timeSeconds, juce::jmax(0.1, cycle));

                if (t < slideDur)
                    x = (int) juce::jmap(t, 0.0, slideDur, (double) -textWidth, (double) centeredX);
                else if (t < slideDur + layer.holdSeconds)
                    x = centeredX;
                else if (t < slideDur * 2.0 + layer.holdSeconds)
                    x = (int) juce::jmap(t - slideDur - layer.holdSeconds, 0.0, slideDur,
                                         (double) centeredX, (double) image.getWidth());
                else
                    return; // hidden during the gap between cycles
                break;
            }
        }

        juce::Rectangle<int> box(x, y, textWidth, boxHeight);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(box.toFloat(), 6.0f);
        g.setColour(juce::Colours::white);
        g.drawText(layer.text, box, juce::Justification::centred, true);
    }

    void drawTextOverlays(juce::Image& image, const VideoExporter::Options& options, double timeSeconds)
    {
        drawSlidingText(image, options.topText, VerticalBand::Top, timeSeconds);
        drawSlidingText(image, options.centerText, VerticalBand::Center, timeSeconds);
        drawSlidingText(image, options.bottomText, VerticalBand::Bottom, timeSeconds);
    }
}

VideoExporter::VideoExporter(Options exportOptions)
    : juce::Thread("video-exporter"), options(std::move(exportOptions))
{
}

VideoExporter::~VideoExporter()
{
    stopThread(5000);
}

void VideoExporter::finishWith(Result r, const juce::String& message)
{
    result = r;
    resultMessage = message;
    finished.store(true);
}

bool VideoExporter::isFfmpegAvailable()
{
    FILE* pipe = openPipe("ffmpeg -version", "r");
    if (pipe == nullptr)
        return false;

    char buf[64];
    const bool gotOutput = fgets(buf, sizeof(buf), pipe) != nullptr;
    const int status = closePipe(pipe);
    return gotOutput && exitedCleanly(status);
}

void VideoExporter::run()
{
    // 0. Fail fast if the chosen output location can't be written to -- otherwise
    // this would only surface as an opaque ffmpeg mux error after the full
    // decode+encode pass (which can take minutes for a whole song).
    const auto outputParent = options.outputFile.getParentDirectory();
    if (!outputParent.isDirectory())
    {
        finishWith(Result::Failed, "Output folder does not exist: " + outputParent.getFullPathName());
        return;
    }
    if (!outputParent.hasWriteAccess())
    {
        finishWith(Result::Failed, "No permission to write to: " + outputParent.getFullPathName());
        return;
    }

    // 1. Build an independent decode source -- never the live playback engine's,
    // so exporting cannot disturb (or be disturbed by) live playback/visualization.
    std::unique_ptr<juce::AudioFormatManager> standaloneFormatManager;
    std::unique_ptr<juce::PositionableAudioSource> source;
    double sampleRate = 44100.0;

    // Typed aliases into `source`, used to query the current pattern/row per frame
    // for PatternGrid/Combined mode -- exactly what AudioEngine::getCurrentPatternRow/
    // Index (AudioEngine.cpp) query for the live UI, just against our own offline source.
    XtpSequencerSource* xtpPtr = nullptr;
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    OpenMptAudioFormatReader* openMptReaderPtr = nullptr;
#endif

    if (options.isXtpSequence)
    {
        auto xtp = std::make_unique<XtpSequencerSource>(options.sourceFile, XtpSequencerSource::kRenderSampleRate);
        if (!xtp->isValid())
        {
            finishWith(Result::Failed, "Failed to load " + options.sourceFile.getFileName()
                       + ": " + xtp->getLoadError());
            return;
        }
        sampleRate = XtpSequencerSource::kRenderSampleRate;
        xtpPtr = xtp.get();
        source = std::move(xtp);
    }
    else
    {
        standaloneFormatManager = AudioEngine::createStandaloneFormatManager();
        auto* reader = standaloneFormatManager->createReaderFor(options.sourceFile);
        if (reader == nullptr)
        {
            finishWith(Result::Failed, "Could not open " + options.sourceFile.getFileName());
            return;
        }
        sampleRate = reader->sampleRate;
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
        openMptReaderPtr = dynamic_cast<OpenMptAudioFormatReader*>(reader);
#endif
        source = std::make_unique<MultiChannelAudioFormatReaderSource>(reader, true);
    }

    auto getCurrentRowPattern = [&]() -> std::pair<int, int> // {pattern, row}
    {
        if (xtpPtr != nullptr)
            return { xtpPtr->getCurrentPatternIndex(), xtpPtr->getCurrentRow() };
#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
        if (openMptReaderPtr != nullptr)
            return { openMptReaderPtr->getCurrentPattern(), openMptReaderPtr->getCurrentRow() };
#endif
        return { -1, -1 };
    };

    // Frame layout, computed once: which rectangle(s) the pattern grid and/or the
    // visualization occupy in the output frame, per mode/layout.
    juce::Rectangle<int> gridBounds, vizBounds;
    computeLayout(options.mode, options.combinedLayout, options.width, options.height, gridBounds, vizBounds);

    // PatternGrid/Combined mode: populate an independent ModulePatternData and a
    // fresh ModulePatternViewer -- fails fast here (before any decode/ffmpeg work)
    // if the track has no pattern data at all (e.g. a plain mp3).
    ModulePatternData patternData;
    std::unique_ptr<ModulePatternViewer> patternViewer;

    if (options.mode != ExportMode::Visualization)
    {
        const bool loaded = options.isXtpSequence ? patternData.loadFromXtpSong(xtpPtr->getSong())
                                                  : patternData.loadFromFile(options.sourceFile);
        if (!loaded || !patternData.isLoaded())
        {
            finishWith(Result::Failed, "This mode requires a tracker module or .xtp/.xtd song.");
            return;
        }

        patternViewer = std::make_unique<ModulePatternViewer>();
        // A baked video frame has no scrollbar, so every channel must fit --
        // rather than being clipped to whatever ~100px-wide columns happen to
        // span the frame width (previously only ~12 channels of a 16+ channel
        // song were visible).
        patternViewer->setFitChannelsToWidth(true);
        patternViewer->setPatternData(&patternData);
        patternViewer->setSize(juce::jmax(1, gridBounds.getWidth()), juce::jmax(1, gridBounds.getHeight()));
    }

    const int samplesPerFrame = juce::jmax(1, (int) std::round(sampleRate / (double) kFps));
    source->prepareToPlay(samplesPerFrame, sampleRate);
    const juce::int64 totalLength = source->getTotalLength();
    if (totalLength <= 0)
    {
        finishWith(Result::Failed, "Track has no audio to export.");
        return;
    }

    // 2. Temp files for the two intermediate ffmpeg passes.
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    auto uid = juce::String::toHexString(juce::Random::getSystemRandom().nextInt64());
    auto tempAudioFile = tempDir.getChildFile("sp2export_" + uid + "_audio.wav");
    auto tempVideoFile = tempDir.getChildFile("sp2export_" + uid + "_video.mp4");
    auto tempLogFile = tempDir.getChildFile("sp2export_" + uid + "_ffmpeg.log");

    auto cleanupTemps = [&]
    {
        tempAudioFile.deleteFile();
        tempVideoFile.deleteFile();
        tempLogFile.deleteFile();
    };

    // ffmpeg is run with -loglevel error, so its stderr -- redirected here into a
    // log file rather than just discarded -- holds the actual failure reason
    // whenever a pipe closes uncleanly; folded into the error message below so a
    // failed export is diagnosable instead of just "ffmpeg failed".
    auto readFfmpegLog = [&]() -> juce::String
    {
        auto log = tempLogFile.loadFileAsString().trim();
        return log.isEmpty() ? juce::String() : (" ffmpeg said: " + log);
    };

    // 3. WAV writer for the audio track (decoded in the same pass as the frames below).
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> audioOutStream(tempAudioFile.createOutputStream());
    if (audioOutStream == nullptr)
    {
        finishWith(Result::Failed, "Could not create a temporary audio file.");
        return;
    }
    std::unique_ptr<juce::AudioFormatWriter> audioWriter(
        wavFormat.createWriterFor(audioOutStream.get(), sampleRate, 2, 16, {}, 0));
    if (audioWriter == nullptr)
    {
        finishWith(Result::Failed, "Could not create a WAV writer for the audio track.");
        return;
    }
    audioOutStream.release(); // the writer now owns the stream

    // 4. Stream raw frames straight into an ffmpeg encode process -- never touches
    // disk as raw frames, which would be tens of GB for a full song at HD res.
    const juce::String videoCmd = "ffmpeg -y -hide_banner -loglevel error -f rawvideo -pix_fmt bgra -s "
        + juce::String(options.width) + "x" + juce::String(options.height)
        + " -r " + juce::String(kFps) + " -i - -an -c:v libx264 -preset veryfast -pix_fmt yuv420p "
        + shellQuote(tempVideoFile.getFullPathName()) + " 2>" + shellQuote(tempLogFile.getFullPathName());

    FILE* videoPipe = openPipe(videoCmd, "w");
    if (videoPipe == nullptr)
    {
        cleanupTemps();
        finishWith(Result::Failed, "Could not start ffmpeg for video encoding.");
        return;
    }

    // 5. Single-pass loop: decode one video frame's worth of audio, analyze it,
    // update/render the plugin, write the audio to the WAV and the frame to ffmpeg.
    juce::Image frameImage(juce::Image::ARGB, options.width, options.height, true);
    auto plugin = options.pluginFactory != nullptr ? options.pluginFactory() : nullptr;
    SpectrumAnalyzer spectrumAnalyzer;

    // Own Equalizer instance (never the live UI's), fed a snapshot of its
    // gains -- same cross-thread-safety reasoning as pluginFactory above.
    Equalizer exportEqualizer;
    exportEqualizer.setAllGains(options.equalizerGainsDb);
    exportEqualizer.prepare(sampleRate, samplesPerFrame, 2);

    juce::AudioBuffer<float> chunk(2, samplesPerFrame);
    juce::int64 position = 0;
    const juce::int64 numFrames = (totalLength + samplesPerFrame - 1) / samplesPerFrame;
    juce::int64 frameIndex = 0;
    bool cancelled = false;

    while (position < totalLength)
    {
        if (threadShouldExit())
        {
            cancelled = true;
            break;
        }

        const juce::int64 remaining = totalLength - position;
        const int n = remaining < (juce::int64) samplesPerFrame ? (int) remaining : samplesPerFrame;
        chunk.clear();
        juce::AudioSourceChannelInfo info(&chunk, 0, n);
        source->getNextAudioBlock(info);
        exportEqualizer.process(chunk, 2, n);

        audioWriter->writeFromAudioSampleBuffer(chunk, 0, n);

        spectrumAnalyzer.analyze(chunk.getReadPointer(0), n);

        const bool isFullOverlay = options.mode == ExportMode::Combined
                                  && options.combinedLayout == CombinedLayout::Overlay;

        {
            juce::Graphics g(frameImage);
            g.fillAll(juce::Colours::black);

            if (!gridBounds.isEmpty() && patternViewer != nullptr)
            {
                const auto patternRow = getCurrentRowPattern();
                patternViewer->setPlaybackPosition(patternRow.first, patternRow.second);

                juce::Image gridImage(juce::Image::ARGB, gridBounds.getWidth(), gridBounds.getHeight(), true);
                juce::Graphics gg(gridImage);
                patternViewer->paintEntireComponent(gg, false);
                g.drawImageAt(gridImage, gridBounds.getX(), gridBounds.getY());
            }

            if (!vizBounds.isEmpty() && plugin != nullptr)
            {
                plugin->update(chunk, spectrumAnalyzer.getSpectrumData());

                juce::Image vizImage(juce::Image::ARGB, vizBounds.getWidth(), vizBounds.getHeight(), true);
                juce::Graphics vg(vizImage);
                vg.fillAll(juce::Colours::black);
                plugin->render(vg, juce::Rectangle<int>(0, 0, vizBounds.getWidth(), vizBounds.getHeight()));

                // In Overlay layout, grid and viz share the whole frame and the grid was
                // just drawn opaquely above -- drawing the viz layer at full alpha here
                // would completely hide it (both fill their bounds opaquely on their own),
                // so blend it in at combinedOverlayOpacity instead.
                if (isFullOverlay)
                    g.setOpacity(options.combinedOverlayOpacity);
                g.drawImageAt(vizImage, vizBounds.getX(), vizBounds.getY());
                if (isFullOverlay)
                    g.setOpacity(1.0f);
            }
        }

        applyColorTheme(frameImage, options.colorTheme);
        drawTextOverlays(frameImage, options, (double) frameIndex / (double) kFps);

        {
            const juce::Image::BitmapData bitmap(frameImage, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < options.height; ++y)
                fwrite(bitmap.getLinePointer(y), 1, (size_t) options.width * kBytesPerPixel, videoPipe);
        }

        position += n;
        ++frameIndex;
        progress.store(numFrames > 0 ? (double) frameIndex / (double) numFrames : 1.0);
    }

    audioWriter.reset(); // flush + close the WAV file

    const int videoPipeStatus = closePipe(videoPipe);

    if (cancelled)
    {
        cleanupTemps();
        finishWith(Result::Cancelled, "Export cancelled.");
        return;
    }

    if (!exitedCleanly(videoPipeStatus))
    {
        const auto log = readFfmpegLog();
        cleanupTemps();
        finishWith(Result::Failed, "ffmpeg failed while encoding video frames." + log);
        return;
    }

    // 6. Mux the video-only file with the rendered audio track into the final output.
    // Format is forced explicitly (-f mp4) rather than left for ffmpeg to guess from
    // the output filename's extension -- some save-dialog paths on Linux (zenity, in
    // particular) can hand back a path with the extension dropped, which otherwise
    // fails muxing with "unable to choose an output format".
    const juce::String muxCmd = "ffmpeg -y -hide_banner -loglevel error -i "
        + shellQuote(tempVideoFile.getFullPathName()) + " -i " + shellQuote(tempAudioFile.getFullPathName())
        + " -c:v copy -c:a aac -shortest -movflags +faststart -f mp4 "
        + shellQuote(options.outputFile.getFullPathName()) + " 2>" + shellQuote(tempLogFile.getFullPathName());

    FILE* muxPipe = openPipe(muxCmd, "r");
    if (muxPipe == nullptr)
    {
        cleanupTemps();
        finishWith(Result::Failed, "Could not start ffmpeg for muxing.");
        return;
    }
    char discard[256];
    while (fgets(discard, sizeof(discard), muxPipe) != nullptr) {}
    const int muxStatus = closePipe(muxPipe);

    if (!exitedCleanly(muxStatus))
    {
        const auto log = readFfmpegLog();
        cleanupTemps();
        finishWith(Result::Failed, "ffmpeg failed while muxing audio and video." + log);
        return;
    }

    cleanupTemps();

    finishWith(Result::Success, "Video exported to " + options.outputFile.getFullPathName());
}
