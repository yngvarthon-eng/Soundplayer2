#pragma once

#include <juce_core/juce_core.h>
#include <cstdio>

#if defined(_WIN32)
#include <stdio.h>
#else
#include <sys/wait.h>
#endif

// Small process-pipe helpers shared by everything that drives ffmpeg as a
// subprocess (VideoExporter's offline renders, ScreenRecorder's live capture).
namespace FfmpegProcess
{
    inline juce::String shellQuote(const juce::String& path)
    {
        return "'" + path.replace("'", "'\\''") + "'";
    }

#if defined(_WIN32)
    inline FILE* openPipe(const juce::String& cmd, const char* mode)  { return _popen(cmd.toRawUTF8(), mode); }
    inline int   closePipe(FILE* f)                                   { return _pclose(f); }
    inline bool  exitedCleanly(int status)                            { return status == 0; }
#else
    inline FILE* openPipe(const juce::String& cmd, const char* mode)  { return popen(cmd.toRawUTF8(), mode); }
    inline int   closePipe(FILE* f)                                   { return pclose(f); }
    inline bool  exitedCleanly(int status)                            { return WIFEXITED(status) && WEXITSTATUS(status) == 0; }
#endif
}
