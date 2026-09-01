cmake_minimum_required(VERSION 3.21)

project(SoundPlayer2 VERSION 1.3.0)

# JUCE CMake Setup
set(JUCE_GITHUB_REPO https://github.com/juce-framework/JUCE.git)
set(JUCE_GITHUB_TAG 7.0.9)

# CLAP SDK (header-only)
include(FetchContent)
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG        1.2.1
)
FetchContent_MakeAvailable(clap)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download JUCE if not present
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    message(STATUS "Downloading JUCE...")
    execute_process(
        COMMAND git clone --branch ${JUCE_GITHUB_TAG} ${JUCE_GITHUB_REPO}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# Add JUCE
add_subdirectory(JUCE)

# Create the audio player app
juce_add_gui_app(SoundPlayer2
    PRODUCT_NAME "SoundPlayer2"
    COMPANY_NAME "SoundPlayer"
    BUNDLE_IDENTIFIER com.soundplayer.soundplayer2
)

# Target sources
target_sources(SoundPlayer2 PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/Audio/AudioEngine.h
    Source/Audio/AudioEngine.cpp
    Source/Audio/FormatManager.h
    Source/Audio/FormatManager.cpp
    Source/Audio/MultiChannelAudioFormatReaderSource.h
    Source/Audio/MultiChannelAudioFormatReaderSource.cpp
    Source/Audio/ModPlugFormat.h
    Source/Audio/ModPlugFormat.cpp
    Source/Xtp/XtpSong.h
    Source/Xtp/XtpSong.cpp
    Source/Xtp/XtpTransportClock.h
    Source/Xtp/XtpInstrumentVoice.h
    Source/Xtp/XtpInstrumentVoice.cpp
    Source/Xtp/XtpSequencer.h
    Source/Xtp/XtpSequencer.cpp
    Source/Xtp/XtpSequencerSource.h
    Source/Xtp/XtpSequencerSource.cpp
    Source/Xtp/XtpSf2Voice.h
    Source/Xtp/XtpSf2Voice.cpp
    Source/Playlist/PlaylistManager.h
    Source/Playlist/PlaylistManager.cpp
    Source/Playlist/PlaylistItem.h
    Source/UI/PlaylistEditorComponent.h
    Source/UI/PlaylistEditorComponent.cpp
    Source/UI/PlayerControlsComponent.h
    Source/UI/PlayerControlsComponent.cpp
    Source/UI/VisualizationComponent.h
    Source/UI/VisualizationComponent.cpp
    Source/UI/FullScreenVisualizerWindow.h
    Source/UI/FullScreenVisualizerWindow.cpp
    Source/UI/FullScreenHostWindow.h
    Source/UI/FullScreenHostWindow.cpp
    Source/UI/ModulePatternViewer.h
    Source/UI/ModulePatternViewer.cpp
    Source/Visualization/ModulePatternData.h
    Source/Visualization/ModulePatternData.cpp
    Source/Effects/EqualizerComponent.h
    Source/Effects/EqualizerComponent.cpp
    Source/Effects/Equalizer.h
    Source/Effects/Equalizer.cpp
    Source/Effects/EqPresetManager.h
    Source/Effects/EqPresetManager.cpp
    Source/Visualization/SpectrumAnalyzer.h
    Source/Visualization/SpectrumAnalyzer.cpp
    Source/Visualization/VisualizationPlugin.h
    Source/Visualization/Plugins/WaveformPlugin.h
    Source/Visualization/Plugins/WaveformPlugin.cpp
    Source/Visualization/Plugins/SpectrumBarsPlugin.h
    Source/Visualization/Plugins/SpectrumBarsPlugin.cpp
    Source/Visualization/Plugins/SpectrogramPlugin.h
    Source/Visualization/Plugins/SpectrogramPlugin.cpp
    Source/Visualization/Plugins/VUMeterPlugin.h
    Source/Visualization/Plugins/VUMeterPlugin.cpp
    Source/Visualization/Plugins/LissajousPlugin.h
    Source/Visualization/Plugins/LissajousPlugin.cpp
    Source/Visualization/Plugins/RadialSpectrumPlugin.h
    Source/Visualization/Plugins/RadialSpectrumPlugin.cpp
    Source/Visualization/Plugins/ParticleBloomPlugin.h
    Source/Visualization/Plugins/ParticleBloomPlugin.cpp
    Source/Visualization/Plugins/BarsPeakHoldPlugin.h
    Source/Visualization/Plugins/BarsPeakHoldPlugin.cpp
    Source/Visualization/Plugins/StarfieldTunnelPlugin.h
    Source/Visualization/Plugins/StarfieldTunnelPlugin.cpp
    Source/Plugins/PluginChain.h
    Source/Plugins/PluginChain.cpp
    Source/Plugins/PluginHostManager.h
    Source/Plugins/PluginHostManager.cpp
    Source/Plugins/ClapPluginFormat.h
    Source/Plugins/ClapPluginFormat.cpp
    Source/Plugins/Lv2PluginFormat.h
    Source/Plugins/Lv2PluginFormat.cpp
    Source/UI/PluginChainComponent.h
    Source/UI/PluginChainComponent.cpp
    Source/Analysis/AudioAnalyzer.h
    Source/Analysis/AudioAnalyzer.cpp
    Source/UI/AudioAnalysisComponent.h
    Source/UI/AudioAnalysisComponent.cpp
    Source/Export/VideoExporter.h
    Source/Export/VideoExporter.cpp
    Source/Export/FfmpegProcess.h
    Source/Export/ScreenRecorder.h
    Source/Export/ScreenRecorder.cpp
    Source/UI/VideoExportSettingsDialog.h
    Source/UI/VideoExportSettingsDialog.cpp
)

# Link JUCE modules
target_link_libraries(SoundPlayer2 PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
)

# Enable JUCE plugin hosting
target_compile_definitions(SoundPlayer2 PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_MP3AUDIOFORMAT=1
)

# Link CLAP SDK headers
target_link_libraries(SoundPlayer2 PRIVATE clap)

# On Linux/Mac, dlopen is needed for CLAP hosting
if(UNIX)
    target_link_libraries(SoundPlayer2 PRIVATE dl)
endif()

# Optional: Add libopenmpt for module format support
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    # Find required libraries
    pkg_check_modules(LIBCURL libcurl)
    if(LIBCURL_FOUND)
        target_link_libraries(SoundPlayer2 PRIVATE ${LIBCURL_LIBRARIES})
        target_include_directories(SoundPlayer2 PRIVATE ${LIBCURL_INCLUDE_DIRS})
    endif()
    
    pkg_check_modules(GTK gtk+-3.0)
    if(GTK_FOUND)
        target_link_libraries(SoundPlayer2 PRIVATE ${GTK_LIBRARIES})
        target_include_directories(SoundPlayer2 PRIVATE ${GTK_INCLUDE_DIRS})
    endif()
    
    # JUCE_WEB_BROWSER=0 is set, so WebKit is not needed
    
    # Optional: libopenmpt for module format support (MOD/XM/IT/S3M/MPTM/etc.)
    pkg_check_modules(OPENMPT libopenmpt)
    if(OPENMPT_FOUND)
        target_link_libraries(SoundPlayer2 PRIVATE ${OPENMPT_LIBRARIES})
        target_include_directories(SoundPlayer2 PRIVATE ${OPENMPT_INCLUDE_DIRS})
        target_compile_definitions(SoundPlayer2 PRIVATE SOUNDPLAYER_OPENMPT_SUPPORT=1)
        message(STATUS "Module format support enabled via libopenmpt")
    endif()

    # Optional: libxmp for broader legacy module compatibility fallback
    pkg_check_modules(XMP libxmp)
    if(XMP_FOUND)
        target_link_libraries(SoundPlayer2 PRIVATE ${XMP_LIBRARIES})
        target_include_directories(SoundPlayer2 PRIVATE ${XMP_INCLUDE_DIRS})
        target_compile_definitions(SoundPlayer2 PRIVATE SOUNDPLAYER_XMP_SUPPORT=1)
        message(STATUS "Module fallback support enabled via libxmp")
    endif()

    # Optional: LV2 hosting via lilv
    pkg_check_modules(LILV lilv-0)
    if(LILV_FOUND)
        target_link_libraries(SoundPlayer2 PRIVATE ${LILV_LIBRARIES})
        target_include_directories(SoundPlayer2 PRIVATE ${LILV_INCLUDE_DIRS})
        target_compile_definitions(SoundPlayer2 PRIVATE SOUNDPLAYER_LV2_SUPPORT=1)
        message(STATUS "LV2 support enabled via lilv")
    endif()

    # Optional: SoundFont (.sf2) instrument playback for .xtp/.xtd songs via FluidSynth
    pkg_check_modules(FLUIDSYNTH fluidsynth)
    if(FLUIDSYNTH_FOUND)
        target_link_libraries(SoundPlayer2 PRIVATE ${FLUIDSYNTH_LIBRARIES})
        target_include_directories(SoundPlayer2 PRIVATE ${FLUIDSYNTH_INCLUDE_DIRS})
        target_compile_definitions(SoundPlayer2 PRIVATE SOUNDPLAYER_FLUIDSYNTH_SUPPORT=1)
        message(STATUS "SF2 instrument support enabled via FluidSynth")
    endif()
endif()

# Set compiler flags
if(MSVC)
    target_compile_options(SoundPlayer2 PRIVATE /W4)
else()
    target_compile_options(SoundPlayer2 PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(SoundPlayer2 PRIVATE)
endif()

# Create feature test executable
add_executable(SoundPlayer2Tests
    Source/Tests/FeatureTests.cpp
    Source/Playlist/PlaylistManager.cpp
    Source/Playlist/PlaylistManager.h
    Source/Audio/AudioEngine.cpp
    Source/Audio/AudioEngine.h
    Source/Audio/FormatManager.cpp
    Source/Audio/FormatManager.h
    Source/Audio/MultiChannelAudioFormatReaderSource.h
    Source/Audio/MultiChannelAudioFormatReaderSource.cpp
    Source/Audio/ModPlugFormat.h
    Source/Audio/ModPlugFormat.cpp
    Source/Xtp/XtpSong.h
    Source/Xtp/XtpSong.cpp
    Source/Xtp/XtpTransportClock.h
    Source/Xtp/XtpInstrumentVoice.h
    Source/Xtp/XtpInstrumentVoice.cpp
    Source/Xtp/XtpSequencer.h
    Source/Xtp/XtpSequencer.cpp
    Source/Xtp/XtpSequencerSource.h
    Source/Xtp/XtpSequencerSource.cpp
    Source/Xtp/XtpSf2Voice.h
    Source/Xtp/XtpSf2Voice.cpp
    Source/Effects/Equalizer.cpp
    Source/Effects/Equalizer.h
    Source/Visualization/ModulePatternData.h
    Source/Visualization/ModulePatternData.cpp
    Source/Visualization/VisualizationPlugin.h
    Source/Visualization/SpectrumAnalyzer.h
    Source/Visualization/SpectrumAnalyzer.cpp
    Source/Visualization/Plugins/WaveformPlugin.h
    Source/Visualization/Plugins/WaveformPlugin.cpp
    Source/Plugins/PluginChain.h
    Source/Plugins/PluginChain.cpp
    Source/UI/ModulePatternViewer.h
    Source/UI/ModulePatternViewer.cpp
    Source/Export/VideoExporter.h
    Source/Export/VideoExporter.cpp
    Source/Export/FfmpegProcess.h
    Source/Export/ScreenRecorder.h
    Source/Export/ScreenRecorder.cpp
)

# Link test executable to JUCE libraries
target_link_libraries(SoundPlayer2Tests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_events
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
)

target_compile_definitions(SoundPlayer2Tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_MP3AUDIOFORMAT=1
)

# Add JUCE dependencies to test
if(PkgConfig_FOUND)
    pkg_check_modules(LIBCURL libcurl)
    if(LIBCURL_FOUND)
        target_link_libraries(SoundPlayer2Tests PRIVATE ${LIBCURL_LIBRARIES})
        target_include_directories(SoundPlayer2Tests PRIVATE ${LIBCURL_INCLUDE_DIRS})
    endif()
    if(GTK_FOUND)
        target_link_libraries(SoundPlayer2Tests PRIVATE ${GTK_LIBRARIES})
        target_include_directories(SoundPlayer2Tests PRIVATE ${GTK_INCLUDE_DIRS})
    endif()
    if(OPENMPT_FOUND)
        target_link_libraries(SoundPlayer2Tests PRIVATE ${OPENMPT_LIBRARIES})
        target_include_directories(SoundPlayer2Tests PRIVATE ${OPENMPT_INCLUDE_DIRS})
        target_compile_definitions(SoundPlayer2Tests PRIVATE SOUNDPLAYER_OPENMPT_SUPPORT=1)
    endif()
    if(XMP_FOUND)
        target_link_libraries(SoundPlayer2Tests PRIVATE ${XMP_LIBRARIES})
        target_include_directories(SoundPlayer2Tests PRIVATE ${XMP_INCLUDE_DIRS})
        target_compile_definitions(SoundPlayer2Tests PRIVATE SOUNDPLAYER_XMP_SUPPORT=1)
    endif()
    if(FLUIDSYNTH_FOUND)
        target_link_libraries(SoundPlayer2Tests PRIVATE ${FLUIDSYNTH_LIBRARIES})
        target_include_directories(SoundPlayer2Tests PRIVATE ${FLUIDSYNTH_INCLUDE_DIRS})
        target_compile_definitions(SoundPlayer2Tests PRIVATE SOUNDPLAYER_FLUIDSYNTH_SUPPORT=1)
    endif()
endif()
