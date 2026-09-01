<!-- Use this file to provide workspace-specific custom instructions to Copilot. For more details, visit https://code.visualstudio.com/docs/copilot/copilot-customization#_use-a-githubcopilotinstructionsmd-file -->

# SoundPlayer2 Development Instructions

## Project Overview
SoundPlayer2 is a JUCE-based audio player supporting standard and module audio formats with playlist management, 10-band equalizer, and real-time visualization.

## Architecture Principles

### Core Components
- **AudioEngine**: Central audio processing; handles playback, format loading, and effects chain
- **PlaylistManager**: Track management with persistence; broadcasts changes to UI
- **UI Components**: Modular components (PlayerControls, PlaylistEditor, Visualization, Equalizer)
- **Effects**: Pluggable processors; Equalizer provided, extensible for future effects

### Design Patterns Used
- Observer Pattern: Components listen to `PlaylistManager` changes via `ChangeListener`
- Callback-based Audio: `AudioIODeviceCallback` for real-time audio processing
- Component Hierarchy: Main UI orchestrates smaller, focused components

## Code Style

### Naming Conventions
- Classes: PascalCase (e.g., `AudioEngine`, `PlaylistManager`)
- Methods: camelCase (e.g., `getCurrentPosition()`)
- Member variables: camelCase with no prefix (e.g., `playlistManager`, `audioBuffer`)
- Constants: UPPERCASE_SNAKE_CASE

### JUCE-Specific Guidelines
- Always use `std::make_unique` for memory management
- Include `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` in all classes
- Use `juce::ChangeBroadcaster` for model-to-view notifications
- Prefer `juce::Array<T>` over `std::vector<T>` for consistency

### File Organization
- Headers (.h): Define class interface with full documentation
- Implementation (.cpp): Keep to one main class per file
- Related files grouped in logical directories: `Audio/`, `Playlist/`, `UI/`, `Effects/`, `Visualization/`

## Building and Testing

### Local Build
```bash
cd /home/yngvar/visualcode/workspace/soundplayer2
mkdir -p build && cd build
cmake ..
cmake --build . --config Debug
```

### Format Support
- **Standard**: MP3, WAV, FLAC, OGG, M4A (via JUCE AudioFormatManager)
- **Modules**: MOD, XM, IT, S3M (via libmodplug, optional)

Enable module support:
```bash
sudo apt-get install libmodplug-dev  # Ubuntu/Debian
# CMake will auto-detect; optional compilation flag SOUNDPLAYER_MODPLUG_SUPPORT
```

## Common Development Tasks

### Adding a New Effect
1. Create header/implementation in `Source/Effects/`
2. Add to `AudioEngine::process()` chain
3. Create UI component for controls in `Source/Effects/`
4. Integrate into `EqualizerComponent` layout or create new effects panel

### Adding a Visualization
1. Extend `VisualizationComponent` or create new in `Source/Visualization/`
2. Implement `timerCallback()` for animation updates
3. Use `audioEngine->getAudioBuffer()` for audio data
4. Add to `MainComponent::resized()` layout

### Supporting New Audio Format
1. Update `FormatManager::getSupportedFormats()` string
2. Add decoder integration in `FormatManager::isSupportedFormat()`
3. Test with `PlaylistEditorComponent` file browser

## Module Song Support

Current state: Modules load as normal tracks in playlist.

Planned enhancements:
- Pattern viewer showing sequencer grid
- Instrument list display
- Sample info panel
- Pattern navigation controls

## Quality Control Settings

**Sample Rates**: 44.1 kHz, 48 kHz, 96 kHz
- Configurable in `PlayerControlsComponent`
- Applied via `AudioEngine::setSampleRate()`

**Bit Depths**: 16-bit, 24-bit, 32-bit
- Configurable in `PlayerControlsComponent`
- Affects output configuration, not current DSP (future enhancement)

## Key Classes and Responsibilities

| Class | File | Responsibility |
|-------|------|-----------------|
| `AudioEngine` | `Audio/AudioEngine.h/cpp` | Playback, format loading, effects processing |
| `PlaylistManager` | `Playlist/PlaylistManager.h/cpp` | Track list management, persistence |
| `PlayerControlsComponent` | `UI/PlayerControlsComponent.h/cpp` | Transport buttons, quality selectors |
| `PlaylistEditorComponent` | `UI/PlaylistEditorComponent.h/cpp` | Track list UI, add/remove/reorder |
| `VisualizationComponent` | `UI/VisualizationComponent.h/cpp` | Real-time waveform and spectrum display |
| `EqualizerComponent` | `Effects/EqualizerComponent.h/cpp` | 10-band EQ UI controls |
| `Equalizer` | `Effects/Equalizer.h/cpp` | EQ DSP processing |

## Known Limitations & TODOs

- [ ] FFT spectrum analysis not fully implemented (`SpectrumAnalyzer.cpp`)
- [ ] Module pattern viewer not yet implemented
- [ ] MIDI keyboard input not implemented
- [ ] Preset save/load for EQ not implemented
- [ ] Cross-fade between tracks not implemented
- [ ] Gapless playback not optimized

## Debugging Tips

1. **Build errors related to JUCE**: Ensure JUCE submodule is present at `./JUCE`
2. **Audio device not found**: Check `AudioEngine::updateAudioDevice()` and system audio settings
3. **UI layout issues**: Verify `MainComponent::resized()` bounds calculations
4. **Memory leaks**: Use Valgrind on Linux or Instruments on macOS
5. **Format loading fails**: Confirm file extensions match `FormatManager` supported list

## External Resources

- [JUCE Framework](https://juce.com)
- [JUCE Audio Guide](https://docs.juce.com/master/module_juce_audio_basics.html)
- [CMake Integration](https://cmake.org/cmake/help/latest/module/FindJUCE.html)
- [libmodplug API](https://sourceforge.net/projects/modplug-xmms/)
