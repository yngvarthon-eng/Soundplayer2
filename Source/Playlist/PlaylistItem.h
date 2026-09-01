#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

struct PlaylistItem
{
    juce::String name;
    juce::File file;
    double duration = 0.0;
    juce::String artist;
    juce::String album;
    juce::String channelLayout;
    bool isModule = false; // True for tracker module formats
    bool isStream = false; // True for HTTP/HTTPS audio streams
    bool isXtpSequence = false; // True for exTracker's native .xtp song format
    juce::String streamUrl;// Non-empty when isStream == true

    PlaylistItem() = default;
    PlaylistItem(const juce::File& f, const juce::String& n = "")
        : name(n.isEmpty() ? f.getFileNameWithoutExtension() : n), file(f), duration(0.0)
    {
        auto ext = f.getFileExtension().toLowerCase();
        isXtpSequence = (ext == ".xtp" || ext == ".xtd");
        isModule = (ext == ".mod" || ext == ".xm" || ext == ".it" || ext == ".s3m"
                    || ext == ".mptm" || ext == ".umx" || ext == ".ahx"
                    || ext == ".ams" || ext == ".amf" || ext == ".med"
                    || ext == ".mo3" || ext == ".mtm" || ext == ".okt"
                    || ext == ".oct" || ext == ".stm" || ext == ".669"
                    || ext == ".umf" || ext == ".drm" || ext == ".digi"
                    || ext == ".far" || ext == ".mdl" || ext == ".mt2"
                    || ext == ".dcm" || ext == ".dlm" || ext == ".ptm"
                    || ext == ".pcm" || ext == ".dbm" || ext == ".dsm"
                    || ext == ".cdm" || ext == ".hvl" || ext == ".c67"
                    || ext == ".sfx" || ext == ".imf" || ext == ".u2b"
                    || ext == ".psm" || ext == ".stp" || ext == ".symmod");
    }

    // Constructor for network streams
    PlaylistItem(const juce::String& url, const juce::String& n)
        : name(n.isEmpty() ? url : n), duration(0.0), isStream(true), streamUrl(url) {}
};
