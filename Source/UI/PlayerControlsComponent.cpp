#include "PlayerControlsComponent.h"

namespace
{
    juce::String formatChannelInfo(int programChannels, int outputChannels)
    {
        juce::String info = " [" + AudioEngine::describeChannelLayout(programChannels);

        if (outputChannels > 0 && outputChannels != programChannels)
            info += " -> " + AudioEngine::describeChannelLayout(outputChannels) + " out";

        info += "]";
        return info;
    }

    juce::String formatStereoExpansionInfo(AudioEngine::StereoExpansionMode mode)
    {
        return mode == AudioEngine::StereoExpansionMode::MultiSpeakerStereo
             ? " [multi-speaker stereo]"
             : juce::String();
    }
}

PlayerControlsComponent::PlayerControlsComponent(AudioEngine* engine, PlaylistManager* playlist)
    : audioEngine(engine), playlistManager(playlist)
{
    // Play/Pause
    playPauseButton = std::make_unique<juce::TextButton>("Play");
    playPauseButton->onClick = [this]
    {
        if (isPlaying)
        {
            audioEngine->pause();
            isPlaying = false;
        }
        else
        {
            audioEngine->play();
            isPlaying = true;
        }
        updatePlayButton();
    };
    addAndMakeVisible(*playPauseButton);

    // Stop
    stopButton = std::make_unique<juce::TextButton>("Stop");
    stopButton->onClick = [this]
    {
        audioEngine->stop();
        isPlaying = false;
        updatePlayButton();
    };
    addAndMakeVisible(*stopButton);

    // Previous
    prevButton = std::make_unique<juce::TextButton>("< Prev");
    prevButton->onClick = [this] { playlistManager->previousTrack(); };
    addAndMakeVisible(*prevButton);

    // Next
    nextButton = std::make_unique<juce::TextButton>("Next >");
    nextButton->onClick = [this] { playlistManager->nextTrack(); };
    addAndMakeVisible(*nextButton);

    // Shuffle (toggle)
    shuffleButton = std::make_unique<juce::TextButton>("Shuffle");
    shuffleButton->setClickingTogglesState(true);
    shuffleButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::steelblue);
    shuffleButton->setToggleState(playlistManager->getShuffle(), juce::dontSendNotification);
    shuffleButton->onClick = [this]
    {
        playlistManager->setShuffle(shuffleButton->getToggleState());
    };
    addAndMakeVisible(*shuffleButton);

    // Repeat (cycles Off -> All -> One)
    repeatButton = std::make_unique<juce::TextButton>("Repeat");
    repeatButton->onClick = [this]
    {
        using RepeatMode = PlaylistManager::RepeatMode;
        switch (playlistManager->getRepeatMode())
        {
            case RepeatMode::Off: playlistManager->setRepeatMode(RepeatMode::All); break;
            case RepeatMode::All: playlistManager->setRepeatMode(RepeatMode::One); break;
            case RepeatMode::One: playlistManager->setRepeatMode(RepeatMode::Off); break;
        }
        updateRepeatButton();
    };
    addAndMakeVisible(*repeatButton);
    updateRepeatButton();

    stereoExpansionButton = std::make_unique<juce::TextButton>("Stereo xN");
    stereoExpansionButton->setClickingTogglesState(true);
    stereoExpansionButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::steelblue);
    stereoExpansionButton->onClick = [this]
    {
        audioEngine->setStereoExpansionMode(
            stereoExpansionButton->getToggleState()
                ? AudioEngine::StereoExpansionMode::MultiSpeakerStereo
                : AudioEngine::StereoExpansionMode::Native);
        updateStereoExpansionButton();
    };
    addAndMakeVisible(*stereoExpansionButton);
    updateStereoExpansionButton();

    speakerTestButton = std::make_unique<juce::TextButton>("Speaker Test");
    speakerTestButton->setClickingTogglesState(true);
    speakerTestButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkorange);
    speakerTestButton->onClick = [this]
    {
        audioEngine->setSpeakerTestEnabled(speakerTestButton->getToggleState());
        updateSpeakerTestButtons();
    };
    addAndMakeVisible(*speakerTestButton);

    speakerTestNextButton = std::make_unique<juce::TextButton>("Next Ch");
    speakerTestNextButton->onClick = [this]
    {
        audioEngine->advanceSpeakerTestChannel();
        updateSpeakerTestButtons();
    };
    addAndMakeVisible(*speakerTestNextButton);
    updateSpeakerTestButtons();

    // Seek bar (full-width, second row)
    positionSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                    juce::Slider::NoTextBox);
    positionSlider->setRange(0.0, 1.0);
    positionSlider->setMouseClickGrabsKeyboardFocus(false);
    positionSlider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
    positionSlider->setColour(juce::Slider::trackColourId, juce::Colours::steelblue);
    positionSlider->onDragStart = [this] { isDraggingSlider = true; };
    positionSlider->onDragEnd = [this]
    {
        isDraggingSlider = false;
        double duration = audioEngine->getTotalDuration();
        if (duration > 0.0)
            audioEngine->setCurrentPosition(positionSlider->getValue() * duration);
    };
    addAndMakeVisible(*positionSlider);

    // Time label
    timeLabel = std::make_unique<juce::Label>("Time", "00:00 / 00:00");
    addAndMakeVisible(*timeLabel);

    // Status label
    statusLabel = std::make_unique<juce::Label>("Status", "No track loaded");
    statusLabel->setFont(juce::Font(15.0f, juce::Font::bold));
    statusLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*statusLabel);

    // Record button
    recordButton = std::make_unique<juce::TextButton>("Record");
    recordButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8b0000));
    recordButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
    recordButton->setClickingTogglesState(false);
    recordButton->onClick = [this]
    {
        if (audioEngine->isRecording())
        {
            audioEngine->stopRecording();
            recordButton->setButtonText("Record");
            recordButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8b0000));
            recordTimeLabel->setVisible(false);
        }
        else
        {
            recordFileChooser = std::make_unique<juce::FileChooser>(
                "Save recording as...",
                juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                    .getChildFile("recording.wav"),
                "*.wav");
            recordFileChooser->launchAsync(
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                [this](const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file != juce::File{})
                    {
                        audioEngine->startRecording(file);
                        recordButton->setButtonText("Stop Rec");
                        recordButton->setColour(juce::TextButton::buttonColourId, juce::Colours::red);
                        recordTimeLabel->setText("REC 0:00", juce::dontSendNotification);
                        recordTimeLabel->setVisible(true);
                    }
                });
        }
    };
    addAndMakeVisible(*recordButton);

    // Recording time label
    recordTimeLabel = std::make_unique<juce::Label>("RecTime", "REC 0:00");
    recordTimeLabel->setColour(juce::Label::textColourId, juce::Colours::red);
    recordTimeLabel->setFont(juce::Font(13.0f, juce::Font::bold));
    recordTimeLabel->setVisible(false);
    addAndMakeVisible(*recordTimeLabel);

    playlistManager->addChangeListener(this);

    startTimer(100); // poll at 10 Hz
}

PlayerControlsComponent::~PlayerControlsComponent()
{
    stopTimer();
    playlistManager->removeChangeListener(this);
}

void PlayerControlsComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}

void PlayerControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(6);

    // Seek bar at bottom
    positionSlider->setBounds(area.removeFromBottom(32));
    area.removeFromBottom(3);

    // Status/title row above seek bar
    statusLabel->setBounds(area.removeFromBottom(20));
    area.removeFromBottom(3);

    // Transport buttons + time label in remaining top space
    auto row = area;
    playPauseButton->setBounds(row.removeFromLeft(70));
    row.removeFromLeft(4);
    stopButton->setBounds(row.removeFromLeft(60));
    row.removeFromLeft(4);
    prevButton->setBounds(row.removeFromLeft(65));
    row.removeFromLeft(4);
    nextButton->setBounds(row.removeFromLeft(65));
    row.removeFromLeft(8);
    shuffleButton->setBounds(row.removeFromLeft(70));
    row.removeFromLeft(4);
    repeatButton->setBounds(row.removeFromLeft(90));
    row.removeFromLeft(4);
    stereoExpansionButton->setBounds(row.removeFromLeft(96));
    row.removeFromLeft(4);
    speakerTestButton->setBounds(row.removeFromLeft(104));
    row.removeFromLeft(4);
    speakerTestNextButton->setBounds(row.removeFromLeft(80));
    row.removeFromLeft(12);
    timeLabel->setBounds(row.removeFromLeft(120));

    // Record controls right-aligned
    recordTimeLabel->setBounds(row.removeFromRight(90));
    row.removeFromRight(4);
    recordButton->setBounds(row.removeFromRight(90));
}

void PlayerControlsComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    isPlaying = audioEngine->isPlaying();
    updatePlayButton();
}

void PlayerControlsComponent::timerCallback()
{
    bool nowPlaying = audioEngine->isPlaying();

    // Auto-advance: was playing, now stopped, and reached the end of the track
    if (isPlaying && !nowPlaying)
    {
        double duration = audioEngine->getTotalDuration();
        double position = audioEngine->getCurrentPlaybackTime();
        if (duration > 0.0 && position >= duration - 0.2)
        {
            if (playlistManager->getRepeatMode() == PlaylistManager::RepeatMode::One)
            {
                // Replay the current track without reloading it.
                audioEngine->setCurrentPosition(0.0);
                audioEngine->play();
                nowPlaying = true;
            }
            else
            {
                // Advances honouring shuffle / repeat-all, or stops at the end
                // of the playlist when repeat is off.
                playlistManager->advanceAfterTrackEnd();
            }
        }
    }

    if (nowPlaying != isPlaying)
    {
        isPlaying = nowPlaying;
        updatePlayButton();
    }

    double duration = audioEngine->getTotalDuration();
    double position = audioEngine->getCurrentPlaybackTime();

    if (duration > 0.0 && !isDraggingSlider)
        positionSlider->setValue(position / duration, juce::dontSendNotification);

    int posSec = (int)position;
    int durSec = (int)duration;
    timeLabel->setText(
        juce::String::formatted("%d:%02d / %d:%02d",
                                posSec / 60, posSec % 60,
                                durSec / 60, durSec % 60),
        juce::dontSendNotification);

    // Update recording time label
    if (audioEngine->isRecording())
    {
        double recSecs = audioEngine->getRecordingDuration();
        int rs = (int)recSecs;
        recordTimeLabel->setText(
            "REC " + juce::String(rs / 60) + ":" + juce::String::formatted("%02d", rs % 60),
            juce::dontSendNotification);
        recordTimeLabel->setVisible(true);
    }

    // Update status line
    auto* item = playlistManager->getCurrentItem();
    if (item != nullptr)
    {
        juce::String status;
        if (audioEngine->isStreamReconnectInProgress())
            status = "Reconnecting stream: ";
        else if (audioEngine->didLastStreamReconnectFail())
            status = "Stream reconnect failed (press Play to retry): ";
        else
            status = isPlaying ? "Playing: " : "Paused: ";

        status += item->name;
        if (item->artist.isNotEmpty())
            status += "  -  " + item->artist;
        status += formatChannelInfo(audioEngine->getCurrentProgramChannels(),
                                    audioEngine->getActiveOutputChannels());
        if (audioEngine->getCurrentProgramChannels() == 2)
            status += formatStereoExpansionInfo(audioEngine->getStereoExpansionMode());
        if (audioEngine->isSpeakerTestEnabled())
            status += " [speaker test ch " + juce::String(audioEngine->getSpeakerTestChannel() + 1) + "]";
        statusLabel->setText(status, juce::dontSendNotification);
    }
    else
    {
        juce::String status = "No track loaded";
        if (audioEngine->isSpeakerTestEnabled())
            status += " [speaker test ch " + juce::String(audioEngine->getSpeakerTestChannel() + 1) + "]";
        statusLabel->setText(status, juce::dontSendNotification);
    }
}

void PlayerControlsComponent::updatePlayButton()
{
    playPauseButton->setButtonText(isPlaying ? "Pause" : "Play");
}

void PlayerControlsComponent::updateRepeatButton()
{
    using RepeatMode = PlaylistManager::RepeatMode;
    const auto mode = playlistManager->getRepeatMode();

    switch (mode)
    {
        case RepeatMode::Off: repeatButton->setButtonText("Repeat");     break;
        case RepeatMode::All: repeatButton->setButtonText("Repeat All"); break;
        case RepeatMode::One: repeatButton->setButtonText("Repeat 1");   break;
    }

    repeatButton->setToggleState(mode != RepeatMode::Off, juce::dontSendNotification);
    repeatButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::steelblue);
}

void PlayerControlsComponent::updateStereoExpansionButton()
{
    const bool enabled = audioEngine->getStereoExpansionMode()
                       == AudioEngine::StereoExpansionMode::MultiSpeakerStereo;
    stereoExpansionButton->setToggleState(enabled, juce::dontSendNotification);
    stereoExpansionButton->setButtonText(enabled ? "Stereo xN On" : "Stereo xN");
}

void PlayerControlsComponent::updateSpeakerTestButtons()
{
    const bool enabled = audioEngine->isSpeakerTestEnabled();
    speakerTestButton->setToggleState(enabled, juce::dontSendNotification);
    speakerTestButton->setButtonText(enabled ? "Test On" : "Speaker Test");
    speakerTestNextButton->setEnabled(enabled);
    speakerTestNextButton->setButtonText("Ch " + juce::String(audioEngine->getSpeakerTestChannel() + 1));
}
