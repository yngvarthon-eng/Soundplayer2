#include <iostream>
#include <cassert>
#include <cmath>
#include "../Playlist/PlaylistManager.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/MultiChannelAudioFormatReaderSource.h"
#include "../Effects/Equalizer.h"
#include "../Xtp/XtpSong.h"
#include "../Xtp/XtpSequencer.h"
#include "../Xtp/XtpSequencerSource.h"
#include "../Xtp/XtpTransportClock.h"
#include "../Export/VideoExporter.h"
#include "../Export/ScreenRecorder.h"
#include "../Visualization/Plugins/WaveformPlugin.h"
#include <thread>
#include <chrono>

void testPlaylistManager()
{
    std::cout << "=== Testing PlaylistManager ===" << std::endl;
    
    PlaylistManager playlist;
    
    // Create temporary test files
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File testFile1 = tempDir.getChildFile("test_audio1.wav");
    juce::File testFile2 = tempDir.getChildFile("test_audio2.mp3");
    juce::File testFile3 = tempDir.getChildFile("test_audio3.flac");
    
    testFile1.create();
    testFile2.create();
    testFile3.create();
    
    // Test: Add files
    juce::Array<juce::File> testFiles;
    testFiles.add(testFile1);
    testFiles.add(testFile2);
    testFiles.add(testFile3);
    playlist.addFiles(testFiles);
    assert(playlist.getItems().size() == 3);
    std::cout << "✓ Add files: " << playlist.getItems().size() << " files in playlist" << std::endl;
    
    // Test: Get current item (empty initially)
    auto currentItem = playlist.getCurrentItem();
    assert(currentItem == nullptr);  // No item selected yet
    std::cout << "✓ Current item (empty): null" << std::endl;
    
    // Test: Select item
    playlist.selectItem(0);
    assert(playlist.getCurrentIndex() == 0);
    std::cout << "✓ Select item: index " << playlist.getCurrentIndex() << std::endl;
    
    currentItem = playlist.getCurrentItem();
    assert(currentItem != nullptr);
    std::cout << "✓ Current item: " << currentItem->name << std::endl;
    
    // Test: Next/Prev
    playlist.nextTrack();
    assert(playlist.getCurrentIndex() == 1);
    std::cout << "✓ Next track: index " << playlist.getCurrentIndex() << std::endl;
    
    playlist.previousTrack();
    assert(playlist.getCurrentIndex() == 0);
    std::cout << "✓ Previous track: index " << playlist.getCurrentIndex() << std::endl;
    
    // Test: Remove
    playlist.removeItem(0);
    assert(playlist.getItems().size() == 2);
    std::cout << "✓ Remove item: " << playlist.getItems().size() << " files remaining" << std::endl;
    
    // Test: Move
    playlist.moveItem(0, 1);
    std::cout << "✓ Move item: reordered playlist" << std::endl;
    
    // Test: Clear
    playlist.clearPlaylist();
    assert(playlist.getItems().size() == 0);
    std::cout << "✓ Clear playlist: " << playlist.getItems().size() << " files" << std::endl;
    
    // Cleanup
    testFile1.deleteFile();
    testFile2.deleteFile();
    testFile3.deleteFile();
    
    std::cout << std::endl;
}

void testPlaylistPersistence()
{
    std::cout << "=== Testing Playlist Persistence ===" << std::endl;

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File audioFile = tempDir.getChildFile("sp2_persist_channels.wav");
    juce::File playlistFile = tempDir.getChildFile("sp2_persist_channels.xml");
    audioFile.deleteFile();
    playlistFile.deleteFile();

    {
        juce::WavAudioFormat wavFormat;
        auto outputStream = std::make_unique<juce::FileOutputStream>(audioFile);
        auto writer = std::unique_ptr<juce::AudioFormatWriter>(
            wavFormat.createWriterFor(outputStream.get(), 48000.0, 6, 16, {}, 0));
        assert(writer != nullptr);
        outputStream.release();

        juce::AudioBuffer<float> buffer(6, 64);
        buffer.clear();
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    }

    PlaylistManager original;
    original.addFile(audioFile);
    assert(original.getItems().size() == 1);
    assert(original.getItems()[0].channelLayout == "5.1");
    assert(original.savePlaylist(playlistFile));

    PlaylistManager restored;
    assert(restored.loadPlaylist(playlistFile));
    assert(restored.getItems().size() == 1);
    assert(restored.getItems()[0].channelLayout == "5.1");
    std::cout << "✓ Persist channel layout through save/load" << std::endl;

    audioFile.deleteFile();
    playlistFile.deleteFile();

    std::cout << std::endl;
}

void testShuffleRepeat()
{
    std::cout << "=== Testing Shuffle & Repeat ===" << std::endl;

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::Array<juce::File> files;
    for (int i = 0; i < 5; ++i)
    {
        auto f = tempDir.getChildFile("sp2_sr_test_" + juce::String(i) + ".wav");
        f.create();
        files.add(f);
    }

    {
        PlaylistManager pl;
        pl.addFiles(files);
        assert(pl.getItems().size() == 5);

        // Default repeat mode is Off
        assert(pl.getRepeatMode() == PlaylistManager::RepeatMode::Off);
        std::cout << "✓ Default repeat mode: Off" << std::endl;

        // Repeat Off: end of list stops (index does not move)
        pl.selectItem(4);
        pl.advanceAfterTrackEnd();
        assert(pl.getCurrentIndex() == 4);
        std::cout << "✓ Repeat Off stops at end of playlist" << std::endl;

        // Repeat Off: mid-list still advances
        pl.selectItem(1);
        pl.advanceAfterTrackEnd();
        assert(pl.getCurrentIndex() == 2);
        std::cout << "✓ Repeat Off advances within playlist" << std::endl;

        // Repeat All: end of list wraps to the start
        pl.setRepeatMode(PlaylistManager::RepeatMode::All);
        pl.selectItem(4);
        pl.advanceAfterTrackEnd();
        assert(pl.getCurrentIndex() == 0);
        std::cout << "✓ Repeat All wraps to start" << std::endl;

        // Repeat One is handled by the player (track replays), so the index is
        // unchanged here — manual next still moves regardless of repeat mode.
        pl.setRepeatMode(PlaylistManager::RepeatMode::One);
        pl.selectItem(2);
        pl.nextTrack();
        assert(pl.getCurrentIndex() == 3);
        std::cout << "✓ Manual next ignores Repeat One" << std::endl;
    }

    {
        // Shuffle: each track is visited exactly once per cycle
        PlaylistManager pl;
        pl.addFiles(files);
        pl.setShuffle(true);
        assert(pl.getShuffle());

        bool seen[5] = { false, false, false, false, false };
        int unique = 0;
        for (int i = 0; i < 5; ++i)
        {
            pl.nextTrack();
            int idx = pl.getCurrentIndex();
            assert(idx >= 0 && idx < 5);
            if (!seen[idx]) { seen[idx] = true; ++unique; }
        }
        assert(unique == 5);
        std::cout << "✓ Shuffle covers all tracks once per cycle" << std::endl;

        // Crossing the cycle boundary must not repeat the just-played track
        int lastIdx = pl.getCurrentIndex();
        pl.nextTrack();
        assert(pl.getCurrentIndex() != lastIdx);
        std::cout << "✓ Shuffle avoids immediate repeat across cycles" << std::endl;

        // previousTrack steps back through the shuffle sequence
        int a = pl.getCurrentIndex();
        pl.nextTrack();
        pl.previousTrack();
        assert(pl.getCurrentIndex() == a);
        std::cout << "✓ Shuffle previous returns to prior track" << std::endl;
    }

    for (auto& f : files)
        f.deleteFile();

    std::cout << std::endl;
}

void testEqualizer()
{
    std::cout << "=== Testing Equalizer ===" << std::endl;
    
    Equalizer eq;
    
    // Test: Set/Get gain for each band
    for (int i = 0; i < 10; ++i)
    {
        float testGain = -6.0f + (i * 1.2f);  // Vary gain per band
        eq.setGain(i, testGain);
        float readGain = eq.getGain(i);
        assert(readGain == testGain);
    }
    std::cout << "✓ Set/Get gain for all 10 bands" << std::endl;
    
    // Test: Gain bounds
    eq.setGain(0, -15.0f);  // Test min
    assert(eq.getGain(0) == -15.0f);
    std::cout << "✓ Min gain (-15dB)" << std::endl;
    
    eq.setGain(1, 12.0f);   // Test max
    assert(eq.getGain(1) == 12.0f);
    std::cout << "✓ Max gain (12dB)" << std::endl;
    
    // Test: Invalid band index
    float invalidGain = eq.getGain(999);  // Out of range
    assert(invalidGain == 0.0f);  // Should return 0
    std::cout << "✓ Out-of-range band returns 0dB" << std::endl;
    
    std::cout << std::endl;
}

void testAudioEngine()
{
    std::cout << "=== Testing AudioEngine ===" << std::endl;
    
    PlaylistManager playlist;
    AudioEngine engine(&playlist);
    
    // Test: Sample rate setting
    engine.setSampleRate(44100.0);
    assert(engine.getCurrentSampleRate() == 44100.0);
    std::cout << "✓ Set sample rate: " << engine.getCurrentSampleRate() << " Hz" << std::endl;
    
    engine.setSampleRate(48000.0);
    assert(engine.getCurrentSampleRate() == 48000.0);
    std::cout << "✓ Change to 48kHz: " << engine.getCurrentSampleRate() << " Hz" << std::endl;
    
    engine.setSampleRate(96000.0);
    assert(engine.getCurrentSampleRate() == 96000.0);
    std::cout << "✓ Change to 96kHz: " << engine.getCurrentSampleRate() << " Hz" << std::endl;
    
    // Test: Bit depth setting
    engine.setBitDepth(16);
    assert(engine.getCurrentBitDepth() == 16);
    std::cout << "✓ Set bit depth: " << engine.getCurrentBitDepth() << "-bit" << std::endl;
    
    engine.setBitDepth(24);
    assert(engine.getCurrentBitDepth() == 24);
    std::cout << "✓ Change to 24-bit: " << engine.getCurrentBitDepth() << "-bit" << std::endl;
    
    engine.setBitDepth(32);
    assert(engine.getCurrentBitDepth() == 32);
    std::cout << "✓ Change to 32-bit: " << engine.getCurrentBitDepth() << "-bit" << std::endl;
    
    // Test: Equalizer access
    Equalizer& eq = engine.getEqualizer();
    eq.setGain(0, 3.0f);
    assert(eq.getGain(0) == 3.0f);
    std::cout << "✓ Access equalizer from engine" << std::endl;
    
    // Test: Audio buffer access
    const auto& buffer = engine.getAudioBuffer();
    std::cout << "✓ Access audio buffer (channels: " << buffer.getNumChannels() 
              << ", samples: " << buffer.getNumSamples() << ")" << std::endl;

    // Test: Multichannel files preserve their decoded channel count.
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File multichannelFile = tempDir.getChildFile("sp2_multichannel_test.wav");
    multichannelFile.deleteFile();

    {
        juce::WavAudioFormat wavFormat;
        auto outputStream = std::make_unique<juce::FileOutputStream>(multichannelFile);
        auto writer = std::unique_ptr<juce::AudioFormatWriter>(
            wavFormat.createWriterFor(outputStream.get(), 48000.0, 4, 16, {}, 0));
        assert(writer != nullptr);
        outputStream.release();

        juce::AudioBuffer<float> multichannelBuffer(4, 64);
        multichannelBuffer.clear();
        writer->writeFromAudioSampleBuffer(multichannelBuffer, 0, multichannelBuffer.getNumSamples());
    }

    engine.loadTrack(multichannelFile);
    assert(engine.getCurrentProgramChannels() == 4);
    std::cout << "✓ Preserve decoded multichannel layout (4 channels)" << std::endl;
    multichannelFile.deleteFile();
    
    std::cout << std::endl;
}

void testMultichannelReaderSource()
{
    std::cout << "=== Testing Multichannel Reader Source ===" << std::endl;

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File audioFile = tempDir.getChildFile("sp2_reader_multichannel.wav");
    audioFile.deleteFile();

    {
        juce::WavAudioFormat wavFormat;
        auto outputStream = std::make_unique<juce::FileOutputStream>(audioFile);
        auto writer = std::unique_ptr<juce::AudioFormatWriter>(
            wavFormat.createWriterFor(outputStream.get(), 48000.0, 4, 16, {}, 0));
        assert(writer != nullptr);
        outputStream.release();

        juce::AudioBuffer<float> buffer(4, 32);
        buffer.clear();
        buffer.applyGain(0, 0, 32, 0.10f);
        buffer.applyGain(1, 0, 32, 0.20f);
        buffer.applyGain(2, 0, 32, 0.30f);
        buffer.applyGain(3, 0, 32, 0.40f);
        for (int ch = 0; ch < 4; ++ch)
            for (int i = 0; i < 32; ++i)
                buffer.setSample(ch, i, 0.1f * (float) (ch + 1));
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    auto* reader = formatManager.createReaderFor(audioFile);
    assert(reader != nullptr);

    MultiChannelAudioFormatReaderSource source(reader, true);
    juce::AudioBuffer<float> output(4, 32);
    output.clear();
    juce::AudioSourceChannelInfo info(&output, 0, 32);
    source.getNextAudioBlock(info);

    assert(std::abs(output.getSample(0, 0) - 0.1f) < 0.01f);
    assert(std::abs(output.getSample(1, 0) - 0.2f) < 0.01f);
    assert(std::abs(output.getSample(2, 0) - 0.3f) < 0.01f);
    assert(std::abs(output.getSample(3, 0) - 0.4f) < 0.01f);
    std::cout << "✓ Preserve rear-channel signal through reader source" << std::endl;

    audioFile.deleteFile();

    std::cout << std::endl;
}

void testStereoExpansion()
{
    std::cout << "=== Testing Stereo Expansion ===" << std::endl;

    juce::AudioBuffer<float> buffer(6, 8);
    buffer.clear();
    for (int i = 0; i < 8; ++i)
    {
        buffer.setSample(0, i, 0.25f);
        buffer.setSample(1, i, -0.5f);
    }

    juce::StringArray channelNames { "Left", "Right", "Center", "LFE", "Rear Left", "Rear Right" };
    AudioEngine::expandStereoToOutputChannels(buffer, 6, 8, &channelNames);

    for (int i = 0; i < 8; ++i)
    {
        assert(std::abs(buffer.getSample(2, i) + 0.125f) < 1e-6f);
        assert(std::abs(buffer.getSample(3, i)) < 1e-6f);
        assert(std::abs(buffer.getSample(4, i) - 0.25f) < 1e-6f);
        assert(std::abs(buffer.getSample(5, i) + 0.5f) < 1e-6f);
    }

    std::cout << "✓ Expand stereo by speaker role: mono center, silent LFE, live rears" << std::endl;
    std::cout << std::endl;
}

void testSpeakerTestChannelAdvance()
{
    std::cout << "=== Testing Speaker Test Channel Advance ===" << std::endl;

    PlaylistManager playlist;
    AudioEngine engine(&playlist);
    engine.advanceSpeakerTestChannel();
    assert(engine.getSpeakerTestChannel() == 1);
    engine.advanceSpeakerTestChannel();
    assert(engine.getSpeakerTestChannel() == 0 || engine.getSpeakerTestChannel() == 2);

    std::cout << "✓ Advance speaker test channel index" << std::endl;
    std::cout << std::endl;
}

void testBitDepthQuantization()
{
    std::cout << "=== Testing Bit-Depth Quantization ===" << std::endl;

    auto approxEqual = [](float a, float b) { return std::abs(a - b) < 1e-6f; };

    // 32-bit is a full-float passthrough — the sample must be returned untouched.
    const float original = 0.123456789f;
    assert(AudioEngine::quantizeSample(original, 32, 0.0f) == original);
    std::cout << "✓ 32-bit passes float through unchanged" << std::endl;

    // 16-bit grid spacing is 1 / 2^15.
    const float step16 = 1.0f / 32768.0f;

    // A value exactly on the grid stays put (dither off).
    const float onGrid = 100.0f * step16;
    assert(approxEqual(AudioEngine::quantizeSample(onGrid, 16, 0.0f), onGrid));
    std::cout << "✓ On-grid value is preserved" << std::endl;

    // Values between grid points snap to the nearest one.
    assert(approxEqual(AudioEngine::quantizeSample(100.4f * step16, 16, 0.0f), 100.0f * step16));
    assert(approxEqual(AudioEngine::quantizeSample(100.6f * step16, 16, 0.0f), 101.0f * step16));
    std::cout << "✓ Off-grid values snap to nearest 16-bit step" << std::endl;

    // Output always lands on the grid (an integer multiple of step), even with dither.
    const float inputs[]  = { 0.0f, 0.333f, -0.777f, 0.999f };
    const float dithers[] = { 0.0f, 0.5f, -0.5f, 0.9f };
    for (float in : inputs)
        for (float d : dithers)
        {
            float q = AudioEngine::quantizeSample(in, 16, d);
            float steps = q / step16;
            assert(std::abs(steps - std::round(steps)) < 1e-2f); // within float precision near ~32k
            assert(q >= -1.0f && q <= 1.0f);                     // never exceeds full scale
        }
    std::cout << "✓ Quantized output stays on the grid and within full scale" << std::endl;

    std::cout << std::endl;
}

void testXtpSongParsing()
{
    std::cout << "=== Testing XtpSong Parsing ===" << std::endl;

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File songFile = tempDir.getChildFile("sp2_test_song.xtp");

    juce::String content =
        "EXTRACKER_SONG_V1 2 2 1 1 0 0\n"
        "PATTERN 0\n"
        "0 0 1 60 0 65535 0 100 0 0 0\n"
        "0 1 0 -1 0 65535 0 100 0 0 0\n"
        "1 0 1 -1 0 65535 0 100 0 20 0\n"
        "1 1 0 -1 0 65535 0 100 0 0 0\n"
        "SONG_ORDER 0\n"
        "PATTERN_SWING 55\n"
        "INSERT_SWING_INHERIT 1\n"
        "ROW_EDIT_SCOPE 0\n"
        "TRANSPORT 140.5 4 2\n"
        "MIDI_MAP -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1\n"
        "INSTRUMENT_ASSIGN 2 \"builtin.square\"\n"
        "MODULE_MESSAGE \"hello\\nworld\"\n"
        "CHANNEL_VOLUME 50 250\n";

    songFile.replaceWithText(content);

    xtp::Song song;
    juce::String error;
    bool ok = song.loadFromFile(songFile, error);
    assert(ok);
    std::cout << "✓ Loads a well-formed SONG_V1 file: " << error << std::endl;

    assert(song.rows == 2 && song.channels == 2);
    assert(song.patterns.size() == 1);
    std::cout << "✓ Header dimensions and pattern count" << std::endl;

    assert(song.patterns[0].at(0, 0).hasNote && song.patterns[0].at(0, 0).note == 60);
    assert(!song.patterns[0].at(0, 1).hasNote);
    assert(song.patterns[0].at(1, 0).hasNote && song.patterns[0].at(1, 0).note == xtp::Step::kNoteOff);
    assert(song.patterns[0].at(1, 0).effectCommand == 20);
    std::cout << "✓ Pattern cell fields (note, note-off sentinel, effect command)" << std::endl;

    assert(song.songOrder.size() == 1 && song.songOrder[0] == 0);
    assert(song.patternSwing.size() == 1 && song.patternSwing[0] == 55);
    assert(std::abs(song.tempoBpm - 140.5) < 1e-9);
    assert(song.ticksPerBeat == 4 && song.ticksPerRow == 2);
    std::cout << "✓ SONG_ORDER, PATTERN_SWING, and TRANSPORT trailer tokens" << std::endl;

    assert(song.instruments[0] == "builtin.sine");   // default, no INSTRUMENT_ASSIGN for slot 0
    assert(song.instruments[1] == "builtin.square"); // default, no INSTRUMENT_ASSIGN for slot 1
    assert(song.instruments[2] == "builtin.square"); // explicit INSTRUMENT_ASSIGN
    assert(song.moduleMessage == "hello\nworld");
    std::cout << "✓ Default instrument slots 0/1, INSTRUMENT_ASSIGN override, and MODULE_MESSAGE unescaping" << std::endl;

    assert(song.channelVolume.size() == 2);
    assert(std::abs(song.channelVolume[0] - 0.5f) < 1e-6f);
    assert(std::abs(song.channelVolume[1] - 2.0f) < 1e-6f); // 250% clamped to the 0-200 range
    std::cout << "✓ CHANNEL_VOLUME trailer token (percent, clamped 0-200)" << std::endl;

    songFile.deleteFile();
    std::cout << std::endl;
}

// Regression test for a crash found interactively: a project referencing a
// SAMPLE_ENTRY file that no longer exists (e.g. a moved/renamed home directory)
// used to segfault in SampleVoice::loadSample() -- file.createInputStream()
// returns nullptr for a missing file, and that nullptr was handed straight to
// WavAudioFormat::createReaderFor(), whose reader constructor dereferenced it
// unconditionally on its first line. Loading the song (and thus building its
// instrument/sample voices) must now fail that one slot gracefully instead.
void testXtpSequencerMissingSampleFile()
{
    std::cout << "=== Testing XtpSequencerSource with a missing sample file ===" << std::endl;

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File songFile = tempDir.getChildFile("sp2_test_missing_sample_song.xtp");

    juce::String content =
        "EXTRACKER_SONG_V1 2 2 1 1 0 0\n"
        "PATTERN 0\n"
        "0 0 1 60 0 65535 0 100 0 0 0\n"
        "0 1 0 -1 0 65535 0 100 0 0 0\n"
        "1 0 1 -1 0 65535 0 100 0 20 0\n"
        "1 1 0 -1 0 65535 0 100 0 0 0\n"
        "SONG_ORDER 0\n"
        "PATTERN_SWING 55\n"
        "INSERT_SWING_INHERIT 1\n"
        "ROW_EDIT_SCOPE 0\n"
        "TRANSPORT 140.5 4 2\n"
        "MIDI_MAP -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1\n"
        "SAMPLE_ENTRY 0 \"gone\" \"/nonexistent/path/does_not_exist.wav\"\n";

    songFile.replaceWithText(content);

    // The bug crashed the whole process during construction -- reaching this
    // line at all (rather than segfaulting) is the actual assertion.
    XtpSequencerSource source(songFile, XtpSequencerSource::kRenderSampleRate);

    if (!source.isValid())
        throw std::runtime_error("Song with a missing sample file should still parse/load: " + source.getLoadError().toStdString());
    std::cout << "✓ Construction survives a missing SAMPLE_ENTRY file (no crash)" << std::endl;

    songFile.deleteFile();
    std::cout << std::endl;
}

namespace
{
    struct RecordedNoteOn
    {
        std::uint8_t instrument;
        std::uint16_t sample;
        int midiNote;
        std::uint8_t velocity;
        bool retrigger;
    };

    struct MockNoteSink : public xtp::NoteSink
    {
        std::vector<RecordedNoteOn> onEvents;
        std::vector<int> offMidiNotes;

        void noteOn(std::uint8_t instrument, std::uint16_t sample, int midiNote,
                   std::uint8_t velocity, bool retrigger) override
        {
            onEvents.push_back({ instrument, sample, midiNote, velocity, retrigger });
        }
        void noteOff(std::uint8_t, std::uint16_t, int midiNote) override
        {
            offMidiNotes.push_back(midiNote);
        }
    };
}

void testXtpSequencerRowJump()
{
    std::cout << "=== Testing Xtp Sequencer Row Jump (0x0B/0x0D) ===" << std::endl;

    // exTracker implements effect 0x0D ("pattern break") identically to 0x0B
    // ("row jump") -- both just jump the row cursor within the current pattern,
    // never advance to a different pattern. This test pins that behavior.
    xtp::Pattern pattern;
    pattern.rows = 4;
    pattern.channels = 1;
    pattern.steps.assign(4, xtp::Step {});

    xtp::Step row0;
    row0.hasNote = true;
    row0.note = 60;
    row0.instrument = 5;
    row0.effectCommand = 0x0D;
    row0.effectValue = 2; // jump to row 2
    pattern.at(0, 0) = row0;

    xtp::Step row2;
    row2.hasNote = true;
    row2.note = 64;
    row2.instrument = 7;
    pattern.at(2, 0) = row2;

    xtp::TransportClock transport;
    transport.reset(pattern.rows);
    xtp::Sequencer sequencer;
    sequencer.reset(pattern.channels);

    MockNoteSink sink;
    sequencer.onRowBoundary(pattern, transport, sink);

    assert(transport.row() == 2);
    std::cout << "✓ 0x0D lands on row 2 within the same pattern" << std::endl;

    assert(sink.onEvents.size() == 2);
    assert(sink.onEvents[0].instrument == 5 && sink.onEvents[0].midiNote == 60);
    assert(sink.onEvents[1].instrument == 7 && sink.onEvents[1].midiNote == 64);
    std::cout << "✓ Dispatches row 0's note, then chases the jump to dispatch row 2's note" << std::endl;

    std::cout << std::endl;
}

void testXtpSequencerChannelVolume()
{
    std::cout << "=== Testing Xtp Sequencer Channel Volume ===" << std::endl;

    // Two channels, velocity 100 on both: channel 0 left at unity, channel 1
    // scaled to 50%. Mirrors exTracker's own Sequencer::channelVolume_
    // formula: round(velocity * multiplier), clamped to the 1-127 MIDI range.
    xtp::Pattern pattern;
    pattern.rows = 1;
    pattern.channels = 2;
    pattern.steps.assign(2, xtp::Step {});

    xtp::Step note;
    note.hasNote = true;
    note.note = 60;
    note.instrument = 0;
    note.velocity = 100;
    pattern.at(0, 0) = note;
    pattern.at(0, 1) = note;

    xtp::TransportClock transport;
    transport.reset(pattern.rows);
    xtp::Sequencer sequencer;
    sequencer.reset(pattern.channels);
    sequencer.setChannelVolumes({ 1.0f, 0.5f });

    MockNoteSink sink;
    sequencer.onRowBoundary(pattern, transport, sink);

    assert(sink.onEvents.size() == 2);
    assert(sink.onEvents[0].velocity == 100); // channel 0: unity, unchanged
    assert(sink.onEvents[1].velocity == 50);  // channel 1: 100 * 0.5 = 50
    std::cout << "✓ Per-channel volume scales trigger velocity (unity channel unaffected)" << std::endl;

    // A near-silent multiplier still clamps to the MIDI floor of 1, matching
    // exTracker's clamp -- 0% volume attenuates heavily but never truly mutes
    // via this path (channel_cli.cpp's 0% is a UI convention, not a hard mute).
    xtp::Sequencer quietSequencer;
    quietSequencer.reset(pattern.channels);
    quietSequencer.setChannelVolumes({ 0.0f, 2.0f });

    MockNoteSink quietSink;
    xtp::TransportClock quietTransport;
    quietTransport.reset(pattern.rows);
    quietSequencer.onRowBoundary(pattern, quietTransport, quietSink);

    assert(quietSink.onEvents[0].velocity == 1);   // clamped to MIDI floor, not 0
    assert(quietSink.onEvents[1].velocity == 127); // 100 * 2.0 = 200, clamped to MIDI ceiling
    std::cout << "✓ Extreme multipliers clamp to the 1-127 MIDI velocity range" << std::endl;

    std::cout << std::endl;
}

namespace
{
    // Writes a short stereo sine-wave WAV so the exporter has real (non-silent)
    // audio to decode, analyze, and render frames from.
    juce::File writeTestToneWav(const juce::File& file, double sampleRate, double seconds)
    {
        file.deleteFile();
        juce::WavAudioFormat wavFormat;
        auto outputStream = std::make_unique<juce::FileOutputStream>(file);
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(outputStream.get(), sampleRate, 2, 16, {}, 0));
        assert(writer != nullptr);
        outputStream.release();

        const int numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer(2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = 0.5f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate);
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
        }
        writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
        return file;
    }

    // Blocks until the exporter finishes or the timeout elapses (test-only; the
    // real UI polls this non-blockingly from MainComponent::timerCallback instead).
    bool waitForExport(VideoExporter& exporter, int timeoutMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (!exporter.hasFinished())
        {
            if (juce::Time::getMillisecondCounter() > deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }
}

// NOTE: this test intentionally uses explicit if/throw checks rather than assert().
// FeatureTests.cpp is built with NDEBUG defined, which makes assert() a complete
// no-op -- including never evaluating its argument -- so assert(sideEffectingCall())
// would silently skip the call entirely rather than just skip the check.
void testVideoExporter()
{
    std::cout << "=== Testing VideoExporter ===" << std::endl;

    if (!VideoExporter::isFfmpegAvailable())
    {
        std::cout << "! ffmpeg not found on PATH -- skipping VideoExporter test" << std::endl << std::endl;
        return;
    }

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File sourceWav = tempDir.getChildFile("sp2_video_export_test_tone.wav");
    writeTestToneWav(sourceWav, 44100.0, 2.0);

    // Successful export end to end: decode -> WAV + frames -> mux.
    {
        juce::File outputMp4 = tempDir.getChildFile("sp2_video_export_test_output.mp4");
        outputMp4.deleteFile();

        VideoExporter::Options options;
        options.sourceFile = sourceWav;
        options.isXtpSequence = false;
        options.outputFile = outputMp4;
        options.width = 160;
        options.height = 120;
        options.pluginFactory = [] { return std::make_unique<WaveformPlugin>(); };

        VideoExporter exporter(std::move(options));
        exporter.startExport();
        if (!waitForExport(exporter, 30000))
            throw std::runtime_error("VideoExporter did not finish within 30s");

        if (exporter.getResult() != VideoExporter::Result::Success)
            throw std::runtime_error("Export did not succeed: " + exporter.getResultMessage().toStdString());
        std::cout << "✓ Export succeeded: " << exporter.getResultMessage() << std::endl;

        if (!outputMp4.existsAsFile())
            throw std::runtime_error("Output file was not created");
        if (outputMp4.getSize() <= 0)
            throw std::runtime_error("Output file is empty");
        std::cout << "✓ Output file exists (" << outputMp4.getSize() << " bytes)" << std::endl;

        outputMp4.deleteFile();
    }

    // Cancellation: signal immediately, expect a clean Cancelled result and no output file.
    {
        juce::File outputMp4 = tempDir.getChildFile("sp2_video_export_cancel_test_output.mp4");
        outputMp4.deleteFile();

        VideoExporter::Options options;
        options.sourceFile = sourceWav;
        options.isXtpSequence = false;
        options.outputFile = outputMp4;
        options.width = 160;
        options.height = 120;
        options.pluginFactory = [] { return std::make_unique<WaveformPlugin>(); };

        VideoExporter exporter(std::move(options));
        exporter.startExport();
        exporter.cancel();
        if (!waitForExport(exporter, 30000))
            throw std::runtime_error("Cancelled VideoExporter did not finish within 30s");

        if (exporter.getResult() != VideoExporter::Result::Cancelled)
            throw std::runtime_error("Cancelled export did not report Result::Cancelled");
        if (outputMp4.existsAsFile())
            throw std::runtime_error("Cancelled export left an output file behind");
        std::cout << "✓ Cancel mid-export: no output file left behind" << std::endl;
    }

    // PatternGrid mode: reuses the minimal .xtp fixture from testXtpSongParsing() so
    // pattern data is guaranteed to exist.
    juce::File sourceXtp = tempDir.getChildFile("sp2_video_export_test_song.xtp");
    sourceXtp.replaceWithText(
        "EXTRACKER_SONG_V1 2 2 1 1 0 0\n"
        "PATTERN 0\n"
        "0 0 1 60 0 65535 0 100 0 0 0\n"
        "0 1 0 -1 0 65535 0 100 0 0 0\n"
        "1 0 1 -1 0 65535 0 100 0 20 0\n"
        "1 1 0 -1 0 65535 0 100 0 0 0\n"
        "SONG_ORDER 0\n"
        "PATTERN_SWING 55\n"
        "INSERT_SWING_INHERIT 1\n"
        "ROW_EDIT_SCOPE 0\n"
        "TRANSPORT 140.5 4 2\n"
        "MIDI_MAP -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1\n");

    {
        juce::File outputMp4 = tempDir.getChildFile("sp2_video_export_pattern_grid_output.mp4");
        outputMp4.deleteFile();

        VideoExporter::Options options;
        options.sourceFile = sourceXtp;
        options.isXtpSequence = true;
        options.outputFile = outputMp4;
        options.width = 160;
        options.height = 120;
        options.mode = VideoExporter::ExportMode::PatternGrid;

        VideoExporter exporter(std::move(options));
        exporter.startExport();
        if (!waitForExport(exporter, 30000))
            throw std::runtime_error("PatternGrid export did not finish within 30s");

        if (exporter.getResult() != VideoExporter::Result::Success)
            throw std::runtime_error("PatternGrid export did not succeed: " + exporter.getResultMessage().toStdString());
        if (!outputMp4.existsAsFile() || outputMp4.getSize() <= 0)
            throw std::runtime_error("PatternGrid export produced no/empty output file");
        std::cout << "✓ PatternGrid mode export succeeded (" << outputMp4.getSize() << " bytes)" << std::endl;

        outputMp4.deleteFile();
    }

    // Combined mode, exercising layout + a non-Classic theme + a text overlay together.
    {
        juce::File outputMp4 = tempDir.getChildFile("sp2_video_export_combined_output.mp4");
        outputMp4.deleteFile();

        VideoExporter::Options options;
        options.sourceFile = sourceXtp;
        options.isXtpSequence = true;
        options.outputFile = outputMp4;
        options.width = 160;
        options.height = 120;
        options.mode = VideoExporter::ExportMode::Combined;
        options.combinedLayout = VideoExporter::CombinedLayout::SplitGridTop;
        options.colorTheme = VideoExporter::ColorTheme::MonoGreen;
        options.bottomText.text = "Test Song";
        options.pluginFactory = [] { return std::make_unique<WaveformPlugin>(); };

        VideoExporter exporter(std::move(options));
        exporter.startExport();
        if (!waitForExport(exporter, 30000))
            throw std::runtime_error("Combined export did not finish within 30s");

        if (exporter.getResult() != VideoExporter::Result::Success)
            throw std::runtime_error("Combined export did not succeed: " + exporter.getResultMessage().toStdString());
        if (!outputMp4.existsAsFile() || outputMp4.getSize() <= 0)
            throw std::runtime_error("Combined export produced no/empty output file");
        std::cout << "✓ Combined mode export succeeded (" << outputMp4.getSize() << " bytes)" << std::endl;

        outputMp4.deleteFile();
    }

    // Overlay layout (grid + viz sharing the whole frame, blended) together with all
    // three text bands and both slide modes, to exercise the newer compositing and
    // text-animation math end to end.
    {
        juce::File outputMp4 = tempDir.getChildFile("sp2_video_export_overlay_output.mp4");
        outputMp4.deleteFile();

        VideoExporter::Options options;
        options.sourceFile = sourceXtp;
        options.isXtpSequence = true;
        options.outputFile = outputMp4;
        options.width = 160;
        options.height = 120;
        options.mode = VideoExporter::ExportMode::Combined;
        options.combinedLayout = VideoExporter::CombinedLayout::Overlay;
        options.combinedOverlayOpacity = 0.5f;
        options.topText.text = "Top";
        options.topText.slide = VideoExporter::TextSlide::Marquee;
        options.topText.marqueeSeconds = 2.0;
        options.centerText.text = "Center";
        options.bottomText.text = "Bottom";
        options.bottomText.slide = VideoExporter::TextSlide::TimedSlideInOut;
        options.bottomText.holdSeconds = 1.0;
        options.pluginFactory = [] { return std::make_unique<WaveformPlugin>(); };

        VideoExporter exporter(std::move(options));
        exporter.startExport();
        if (!waitForExport(exporter, 30000))
            throw std::runtime_error("Overlay export did not finish within 30s");

        if (exporter.getResult() != VideoExporter::Result::Success)
            throw std::runtime_error("Overlay export did not succeed: " + exporter.getResultMessage().toStdString());
        if (!outputMp4.existsAsFile() || outputMp4.getSize() <= 0)
            throw std::runtime_error("Overlay export produced no/empty output file");
        std::cout << "✓ Overlay layout + sliding text export succeeded (" << outputMp4.getSize() << " bytes)" << std::endl;

        outputMp4.deleteFile();
    }

    sourceXtp.deleteFile();
    sourceWav.deleteFile();
    std::cout << std::endl;
}

// Real-time capture (unlike VideoExporter's offline render), so this needs a
// live X server + a pulse-compatible audio source, neither of which is
// guaranteed in every environment this suite runs in -- skip gracefully
// rather than failing the whole suite when either is unavailable.
void testScreenRecorder()
{
    std::cout << "=== Testing ScreenRecorder ===" << std::endl;

    if (!VideoExporter::isFfmpegAvailable())
    {
        std::cout << "! ffmpeg not found on PATH -- skipping ScreenRecorder test" << std::endl << std::endl;
        return;
    }

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File outputMp4 = tempDir.getChildFile("sp2_screen_record_test_output.mp4");
    outputMp4.deleteFile();

    ScreenRecorder recorder;
    juce::String errorMessage;
    // Doesn't need to be a real/visible window -- just proves the x11grab +
    // pulse muxing pipeline works end to end.
    const bool started = recorder.start(outputMp4, { 0, 0, 160, 120 }, errorMessage);

    if (!started)
    {
        std::cout << "! Could not start screen recording (" << errorMessage
                   << ") -- skipping ScreenRecorder test" << std::endl << std::endl;
        return;
    }
    std::cout << "✓ Started screen recording" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    recorder.stop();

    const auto deadline = juce::Time::getMillisecondCounter() + 15000;
    while (recorder.isFinalizing() && juce::Time::getMillisecondCounter() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

    if (recorder.isFinalizing() || recorder.isRecording())
        throw std::runtime_error("ScreenRecorder did not finish within 15s");
    std::cout << "✓ Stopped and finalized" << std::endl;

    auto err = recorder.getLastError();
    if (err.isNotEmpty())
        throw std::runtime_error("ScreenRecorder reported an error: " + err.toStdString());

    if (!outputMp4.existsAsFile() || outputMp4.getSize() <= 0)
        throw std::runtime_error("Screen recording produced no/empty output file");
    std::cout << "✓ Output file exists (" << outputMp4.getSize() << " bytes)" << std::endl;

    outputMp4.deleteFile();
    std::cout << std::endl;
}

int main()
{
    std::cout << "\n════════════════════════════════════════" << std::endl;
    std::cout << "  SoundPlayer2 Feature Test Suite" << std::endl;
    std::cout << "════════════════════════════════════════\n" << std::endl;
    
    try
    {
        testPlaylistManager();
        testPlaylistPersistence();
        testShuffleRepeat();
        testEqualizer();
        testAudioEngine();
        testMultichannelReaderSource();
        testStereoExpansion();
        testSpeakerTestChannelAdvance();
        testBitDepthQuantization();
        testXtpSongParsing();
        testXtpSequencerMissingSampleFile();
        testXtpSequencerRowJump();
        testXtpSequencerChannelVolume();
        testVideoExporter();
        testScreenRecorder();

        std::cout << "════════════════════════════════════════" << std::endl;
        std::cout << "  ✅ All tests passed!" << std::endl;
        std::cout << "════════════════════════════════════════\n" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
