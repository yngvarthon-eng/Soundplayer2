#include "ScreenRecorder.h"
#include "FfmpegProcess.h"
#include "VideoExporter.h"
#include <cstdio>

using namespace FfmpegProcess;

namespace
{
    constexpr int kScreenRecordFps = 25;

    // Parses `ffmpeg -sources pulse` for the line marked as the default
    // ("* <name> [...]") and returns just the device name (e.g.
    // "alsa_output.pci-....monitor"), or empty if none could be determined.
    // Deliberately goes through ffmpeg itself rather than `pactl` -- this
    // avoids a dependency on pulseaudio-utils being installed, which it
    // isn't on every pipewire-pulse system.
    juce::String findDefaultPulseMonitor()
    {
        FILE* p = openPipe("ffmpeg -hide_banner -sources pulse 2>/dev/null", "r");
        if (p == nullptr)
            return {};

        juce::String defaultName;
        char buf[512];
        while (std::fgets(buf, sizeof(buf), p) != nullptr)
        {
            juce::String line(buf);
            if (line.trimStart().startsWith("*"))
            {
                defaultName = line.fromFirstOccurrenceOf("*", false, false)
                                  .trim()
                                  .upToFirstOccurrenceOf(" ", false, false);
                break;
            }
        }
        closePipe(p);
        return defaultName;
    }
}

ScreenRecorder::ScreenRecorder() = default;

ScreenRecorder::~ScreenRecorder()
{
    // Only trigger a stop ourselves if nobody already asked for one -- if a
    // finalize is already in flight, just wait for it rather than racing it.
    if (recording.load() && !finalizing.load())
        stop();

    if (finalizeThread.joinable())
        finalizeThread.join();
}

bool ScreenRecorder::start(const juce::File& outputFile, juce::Rectangle<int> captureBounds,
                            juce::String& errorMessage)
{
    if (recording.load() || finalizing.load())
    {
        errorMessage = "A recording is already in progress.";
        return false;
    }

    if (!VideoExporter::isFfmpegAvailable())
    {
        errorMessage = "ffmpeg was not found on PATH.";
        return false;
    }

    const auto monitorDevice = findDefaultPulseMonitor();
    if (monitorDevice.isEmpty())
    {
        errorMessage = "Could not determine the default audio output's monitor source "
                        "(\"ffmpeg -sources pulse\" returned nothing).";
        return false;
    }

    // libx264 with yuv420p needs even width/height.
    const int width  = captureBounds.getWidth()  & ~1;
    const int height = captureBounds.getHeight() & ~1;
    if (width <= 0 || height <= 0)
    {
        errorMessage = "Invalid capture region.";
        return false;
    }

    auto display = juce::SystemStats::getEnvironmentVariable("DISPLAY", ":0.0");
    if (!display.contains("."))
        display += ".0";

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    auto uid = juce::String::toHexString(juce::Random::getSystemRandom().nextInt64());
    logFile = tempDir.getChildFile("sp2screenrec_" + uid + "_ffmpeg.log");

    const juce::String grabTarget = display + "+" + juce::String(captureBounds.getX())
                                   + "," + juce::String(captureBounds.getY());

    const juce::String cmd = "ffmpeg -y -hide_banner -loglevel error "
        "-thread_queue_size 1024 -f x11grab -video_size " + juce::String(width) + "x" + juce::String(height)
        + " -framerate " + juce::String(kScreenRecordFps) + " -i " + shellQuote(grabTarget)
        + " -thread_queue_size 1024 -f pulse -i " + shellQuote(monitorDevice)
        + " -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a aac -b:a 192k "
        + shellQuote(outputFile.getFullPathName()) + " 2>" + shellQuote(logFile.getFullPathName());

    pipe = openPipe(cmd, "w");
    if (pipe == nullptr)
    {
        errorMessage = "Could not start ffmpeg for screen recording.";
        return false;
    }

    startTime = juce::Time::getCurrentTime();
    recording = true;
    return true;
}

void ScreenRecorder::stop()
{
    if (!recording.load() || finalizing.load())
        return;

    // ffmpeg treats 'q' on stdin as a graceful-quit request and finalizes the
    // mp4's moov atom correctly; killing the process instead risks an
    // unplayable file.
    if (pipe != nullptr)
    {
        std::fputs("q\n", pipe);
        std::fflush(pipe);
    }

    finalizing = true;

    if (finalizeThread.joinable())
        finalizeThread.join();

    finalizeThread = std::thread([this]
    {
        auto* p = pipe;
        pipe = nullptr;
        const int status = p != nullptr ? closePipe(p) : -1;

        if (!exitedCleanly(status))
        {
            auto log = logFile.loadFileAsString().trim();
            juce::ScopedLock sl(lastErrorLock);
            lastError = log.isNotEmpty() ? ("ffmpeg said: " + log)
                                          : juce::String("Screen recording did not finish cleanly.");
        }

        finalizing = false;
        recording = false;
    });
}

double ScreenRecorder::getRecordingDuration() const
{
    if (!recording.load())
        return 0.0;
    return (juce::Time::getCurrentTime() - startTime).inSeconds();
}

juce::String ScreenRecorder::getLastError() const
{
    juce::ScopedLock sl(lastErrorLock);
    return lastError;
}
