#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <atomic>
#include <cstdio>
#include <thread>

// Real-time capture of a screen region (in practice, the app's own window)
// plus the system's live audio, muxed by ffmpeg into a single mp4 -- unlike
// VideoExporter, which renders offline from a decoded file, this drives a
// long-running ffmpeg subprocess for however long the user is recording.
//
// start()/isRecording() are synchronous; stop() is not -- finalizing the mp4
// means waiting for the ffmpeg child to exit, which is done on a background
// thread so the caller (the UI thread) never blocks. Poll isFinalizing() to
// know when it's actually done.
class ScreenRecorder
{
public:
    ScreenRecorder();
    ~ScreenRecorder();

    // captureBounds is in screen coordinates, sampled once by the caller --
    // not re-sampled if the window is later moved or resized.
    bool start(const juce::File& outputFile, juce::Rectangle<int> captureBounds,
               juce::String& errorMessage);

    void stop();

    bool isRecording() const   { return recording.load(); }
    bool isFinalizing() const  { return finalizing.load(); }
    double getRecordingDuration() const;
    juce::String getLastError() const;

private:
    FILE* pipe = nullptr;
    juce::File logFile;
    juce::Time startTime;
    std::atomic<bool> recording { false };
    std::atomic<bool> finalizing { false };
    juce::String lastError;
    juce::CriticalSection lastErrorLock;
    std::thread finalizeThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenRecorder)
};
