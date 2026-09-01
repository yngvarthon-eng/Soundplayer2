# SoundPlayer2 - A Modern Audio Player with JUCE

A feature-rich audio player built with JUCE, supporting standard and module audio formats with an intuitive playlist editor, equalizer effects, and audio visualizations.

Current Version: 1.3.0

## Features

### Current Implementation
- **Playlist Management**: Drag-and-drop files/folders onto the window, add/remove tracks, save/load playlists
- **Audio Format Support**: MP3, WAV, FLAC, OGG, and tracker/module formats via OpenMPT with libxmp fallback
- **Playback Controls**: Play, pause, stop, next, previous with position seeking
- **Shuffle & Repeat**: Random play order plus repeat Off / All / One
- **10-Band Equalizer**: Adjust audio across 10 frequency bands (31 Hz - 16 kHz)
- **Real-time Visualization**: Waveform, spectrum bars, spectrogram, VU meter, and Lissajous plugins
- **Video Export**: Render the visualization, the tracker pattern grid, or both combined to an MP4 for the full length of a track, with a choice of color theme and an optional text overlay (requires `ffmpeg` on PATH)
- **Playback Quality Control**: 
  - Sample rates: offered per the output device's actual capabilities (e.g. 44.1/48/96/192 kHz)
  - Bit depths: 16-bit, 24-bit, 32-bit (dithered output requantization; 32-bit = full float)

### Planned Features
- Module song pattern viewer
- Advanced visualization plugins
- MIDI keyboard support
- Preset management for EQ
- Cross-platform builds (Linux, Windows, macOS)
- Plugin wrapper (CLAP/VST3/LV2)

## Project Structure

```
Source/
├── Main.cpp                    # Application entry point
├── MainComponent.h/cpp         # Main UI layout
├── Audio/
│   ├── AudioEngine.h/cpp       # Core playback engine
│   └── FormatManager.h/cpp     # Audio format handling
├── Playlist/
│   ├── PlaylistManager.h/cpp   # Playlist management
│   └── PlaylistItem.h          # Track metadata
├── UI/
│   ├── PlayerControlsComponent # Playback & quality controls
│   ├── PlaylistEditorComponent # Track list & editor
│   └── VisualizationComponent  # Real-time waveform/spectrum
├── Effects/
│   ├── Equalizer.h/cpp         # EQ processing
│   └── EqualizerComponent.h/cpp # EQ UI
└── Visualization/
    └── SpectrumAnalyzer.h/cpp  # FFT-based spectrum analysis
```

## Building

### Prerequisites
- CMake 3.21+
- C++17 compatible compiler (GCC, Clang, or MSVC)
- JUCE framework (automatically downloaded via CMake)
- Optional: libopenmpt (primary module decoder) and libxmp (fallback decoder)
- Optional: `ffmpeg` on PATH, for the Export Video feature

### Build Steps

#### Linux
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
./SoundPlayer2_artefacts/SoundPlayer2
```

#### macOS
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
open SoundPlayer2.app
```

#### Windows
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
SoundPlayer2.exe
```

### Running

```bash
# From the build directory
./SoundPlayer2_artefacts/SoundPlayer2
```

### Optional Dependencies

**libopenmpt + libxmp** (for broad module/tracker support with fallback)
```bash
# Ubuntu/Debian
sudo apt-get install libopenmpt-dev libxmp-dev

# macOS
brew install libopenmpt libxmp

# Fedora/RHEL
sudo dnf install libopenmpt-devel libxmp-devel
```

## Desktop Integration (Linux)

To add a menu entry and associate audio/module files with SoundPlayer2, build the
app first, then run the installer:

```bash
# After building (see above)
bash packaging/install-desktop.sh
```

This installs into your user profile (no sudo):
- a launcher symlink → `~/.local/bin/soundplayer2`
- an application menu entry (`SoundPlayer2`)
- file associations for MP3, WAV, FLAC, OGG and module formats (MOD, XM, IT, S3M)

Double-clicking an associated file (or running `soundplayer2 <file>`) adds it to
the playlist and starts playback. Opening a file while the app is running routes
it into the existing window (single-instance). The launcher points at your newest
local build artifact, so rebuilding in the same build directory updates what the
desktop entry launches automatically. Re-run the script if you switch build
directories or move the repo.

To remove it: `bash packaging/uninstall-desktop.sh`.

## Architecture

### AudioEngine
Central audio processing hub handling:
- Format loading and decoding
- Real-time playback through system audio device
- Sample rate and bit-depth management
- Effects chain processing

### PlaylistManager
Manages tracks with:
- Add/remove operations
- Metadata handling
- Persistence (save/load)
- Change notifications to UI

### Visualizations
- **Waveform Display**: Shows audio amplitude in real-time
- **Spectrum Analyzer**: Frequency-domain visualization using FFT

### Effects
- **10-Band Equalizer**: Adjustable gain per frequency band

## Usage

1. **Add Tracks**: Click "Add Files", or drag audio files/folders onto the window
2. **Playback**: Use Play/Pause/Stop buttons or double-click a track
3. **Shuffle/Repeat**: Toggle "Shuffle" for random order; click "Repeat" to cycle Off → All → One
4. **Adjust Quality**: Select sample rate and bit depth from dropdowns
5. **Equalize**: Drag frequency band sliders to adjust sound
6. **Save Playlist**: Right-click to save current playlist (XML format)

## Development Roadmap

### Phase 1 (Current)
- ✅ Core playback engine
- ✅ Playlist editor
- ✅ Basic visualization
- ✅ 10-band equalizer

### Phase 2
- [x] Module pattern viewer
- [ ] Advanced visualization plugins
- [x] Preset management
- [ ] MIDI keyboard support

### Phase 3
- [ ] Cross-platform optimization
- [ ] Plugin wrapper (CLAP/VST3/LV2)
- [ ] Audio analysis tools
- [ ] Streaming support

## Contributing

To extend SoundPlayer2:

1. **Add Effects**: Create new classes in `Source/Effects/` inheriting from effect interface
2. **Add Visualizations**: Extend `VisualizationComponent` or create new visualization plugins
3. **Add Formats**: Integrate decoders in `FormatManager`
4. **UI Enhancement**: Modify components in `Source/UI/`

## License

TBD - Choose your preferred license (MIT, GPL, Apache 2.0)

## References

- [JUCE Documentation](https://docs.juce.com)
- [libopenmpt](https://lib.openmpt.org/libopenmpt/)
- [libxmp](https://xmp.sourceforge.net/)
- [Audio Processing with JUCE](https://docs.juce.com/master/tutorial_audio_processor.html)
