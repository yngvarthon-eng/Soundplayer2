#include "FormatManager.h"

FormatManager::FormatManager()
{
    formatManager.registerBasicFormats();
}

bool FormatManager::isSupportedFormat(const juce::File& file) const
{
    auto extension = file.getFileExtension().toLowerCase();
    
    // Standard formats
    if (extension == ".mp3" || extension == ".wav" || extension == ".flac" || 
        extension == ".ogg" || extension == ".m4a" || extension == ".aif" ||
        extension == ".aiff")
        return true;

    // Module formats (if libopenmpt is available)
        #if defined(SOUNDPLAYER_OPENMPT_SUPPORT) || defined(SOUNDPLAYER_XMP_SUPPORT)
    if (extension == ".mod" || extension == ".xm" || extension == ".it" ||
        extension == ".s3m" || extension == ".mptm" || extension == ".umx" ||
        extension == ".ahx" || extension == ".ams" || extension == ".amf" ||
        extension == ".med" || extension == ".mo3" || extension == ".mtm" ||
        extension == ".okt" || extension == ".oct" || extension == ".stm" ||
        extension == ".669" || extension == ".umf" || extension == ".drm" ||
        extension == ".digi" || extension == ".far" || extension == ".mdl" ||
        extension == ".mt2" || extension == ".dcm" || extension == ".dlm" ||
        extension == ".ptm" || extension == ".pcm" || extension == ".dbm" ||
        extension == ".dsm" || extension == ".cdm" || extension == ".hvl" ||
        extension == ".c67" || extension == ".sfx" || extension == ".imf" ||
        extension == ".u2b" || extension == ".psm" || extension == ".stp" ||
        extension == ".symmod")
        return true;
        #endif

    // exTracker's native song format (.xtp for pattern exports, .xtd for full
    // songs -- same on-disk grammar either way) -- live-sequenced, not a
    // decodable stream, so it's handled by XtpSequencerSource rather than this
    // AudioFormatManager, but it's still a "supported format" from the
    // file-open dialog's perspective.
    if (extension == ".xtp" || extension == ".xtd")
        return true;

    return false;
}

juce::String FormatManager::getSupportedFormats() const
{
    return "*.mp3;*.wav;*.flac;*.ogg;*.m4a;*.aif;*.aiff;*.xtp;*.xtd;*.mod;*.xm;*.it;*.s3m;*.mptm;*.umx;*.ahx;*.ams;*.amf;*.med;*.mo3;*.mtm;*.okt;*.oct;*.stm;*.669;*.umf;*.drm;*.digi;*.far;*.mdl;*.mt2;*.dcm;*.dlm;*.ptm;*.pcm;*.dbm;*.dsm;*.cdm;*.hvl;*.c67;*.sfx;*.imf;*.u2b;*.psm;*.stp;*.symmod";
}
