#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/AudioEngine.h"
#include "../Playlist/PlaylistManager.h"
#include "../Export/ScreenRecorder.h"

class PlayerControlsComponent : public juce::Component,
                               public juce::ChangeListener,
                               public juce::Timer
{
public:
    PlayerControlsComponent(AudioEngine* engine, PlaylistManager* playlist);
    ~PlayerControlsComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    // Set by MainComponent after construction -- this component has no
    // MainComponent reference of its own, so "record video via the export
    // dialog" is routed out through this callback instead.
    std::function<void()> onRequestVideoExport;

private:
    void updatePlayButton();
    void updateRepeatButton();
    void updateStereoExpansionButton();
    void updateSpeakerTestButtons();

    void showRecordMenu();
    void beginRawAudioRecording();
    void beginScreenRecording();
    void stopActiveRecording();
    void setRecordingUiActive(bool active);

    enum class ActiveRecording { None, RawAudio, Screen };
    ActiveRecording activeRecording = ActiveRecording::None;
    std::unique_ptr<ScreenRecorder> screenRecorder;

    bool isDraggingSlider = false;

    AudioEngine* audioEngine;
    PlaylistManager* playlistManager;

    std::unique_ptr<juce::TextButton> playPauseButton;
    std::unique_ptr<juce::TextButton> stopButton;
    std::unique_ptr<juce::TextButton> prevButton;
    std::unique_ptr<juce::TextButton> nextButton;
    std::unique_ptr<juce::TextButton> shuffleButton;
    std::unique_ptr<juce::TextButton> repeatButton;
    std::unique_ptr<juce::TextButton> stereoExpansionButton;
    std::unique_ptr<juce::TextButton> speakerTestButton;
    std::unique_ptr<juce::TextButton> speakerTestNextButton;
    std::unique_ptr<juce::TextButton> recordButton;

    std::unique_ptr<juce::Slider> positionSlider;
    std::unique_ptr<juce::Label> timeLabel;
    std::unique_ptr<juce::Label> statusLabel;
    std::unique_ptr<juce::Label> recordTimeLabel;

    std::unique_ptr<juce::FileChooser> recordFileChooser;

    bool isPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerControlsComponent)
};
