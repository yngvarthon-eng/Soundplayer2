#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include "../Effects/Equalizer.h"
#include "../Visualization/VisualizationPlugin.h"

// Renders a visualization plugin's output over the full length of a track
// (regular audio file or .xtp/.xtd exTracker song) into a video file, offline
// and frame-stepped rather than tied to live playback or a screen recording.
//
// Runs on a background juce::Thread; the caller polls getProgress()/hasFinished()
// (there is no callback -- avoids any ordering ambiguity between thread exit and
// message-thread delivery).
class VideoExporter : public juce::Thread
{
public:
    enum class Result { Success, Cancelled, Failed };

    // What to render into the frame.
    enum class ExportMode { Visualization, PatternGrid, Combined };

    // Arrangement used when ExportMode::Combined; ignored otherwise. Overlay is the
    // only layout where the grid and visualization share the whole frame rather than
    // being split or inset -- see combinedOverlayOpacity below for why that needs a
    // blend rather than a plain draw.
    enum class CombinedLayout { SplitGridTop, SplitVizTop, PipViz, PipGrid, Overlay };

    // Whole-frame post-process colour treatment, applied uniformly regardless of
    // mode so none of the existing plugins'/grid's hardcoded juce::Colour values
    // need to change.
    enum class ColorTheme { Classic, MonoGreen, MonoAmber, Inverted };

    // How a text layer's horizontal position behaves over time. Static sits centred
    // in its band for the whole export; Marquee scrolls continuously right-to-left
    // and loops forever; TimedSlideInOut slides in, holds, slides out, waits, repeats.
    enum class TextSlide { Static, Marquee, TimedSlideInOut };

    struct TextLayer
    {
        juce::String text; // empty = layer is not drawn at all
        TextSlide slide = TextSlide::Static;
        double marqueeSeconds = 8.0; // Marquee only: seconds for one full off-screen-to-off-screen crossing
        double holdSeconds = 3.0;    // TimedSlideInOut only: seconds held centred between the slide in/out
    };

    struct Options
    {
        juce::File sourceFile;
        bool isXtpSequence = false;
        juce::File outputFile;
        int width = 1280;
        int height = 720;

        ExportMode mode = ExportMode::Visualization;
        CombinedLayout combinedLayout = CombinedLayout::SplitGridTop;
        // Opacity the visualization layer is drawn at when combinedLayout == Overlay
        // (drawn on top of the opaque pattern grid). Ignored for every other layout,
        // which never has the two layers share the same pixels.
        float combinedOverlayOpacity = 0.55f;
        ColorTheme colorTheme = ColorTheme::Classic;

        // Independent optional text layers -- any/all may be empty, in which case
        // that band is simply not drawn.
        TextLayer topText;
        TextLayer centerText;
        TextLayer bottomText;

        // Snapshot of the live UI's 10-band EQ gains at the moment export was
        // started, applied to the exported audio track. All-zero (the default)
        // means no EQ coloration. A snapshot rather than a shared Equalizer
        // instance, for the same cross-thread-state reason as pluginFactory below.
        std::array<float, Equalizer::numBands> equalizerGainsDb {};

        // Invoked once on the export thread to obtain a fresh plugin instance --
        // never the live UI's instance, so there is no cross-thread state sharing.
        // Used whenever `mode` includes the visualization.
        std::function<std::unique_ptr<VisualizationPlugin>()> pluginFactory;
    };

    explicit VideoExporter(Options exportOptions);
    ~VideoExporter() override;

    // True if a working `ffmpeg` binary is reachable on PATH.
    static bool isFfmpegAvailable();

    void startExport() { startThread(); }
    void cancel() { signalThreadShouldExit(); }

    bool hasFinished() const { return finished.load(); }
    double getProgress() const { return progress.load(); }
    Result getResult() const { return result; }
    const juce::String& getResultMessage() const { return resultMessage; }

private:
    void run() override;
    void finishWith(Result r, const juce::String& message);

    Options options;
    std::atomic<double> progress { 0.0 };
    std::atomic<bool> finished { false };
    Result result = Result::Failed;
    juce::String resultMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoExporter)
};
