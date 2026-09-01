#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "PlaylistItem.h"

class PlaylistManager : public juce::ChangeBroadcaster
{
public:
    enum class RepeatMode
    {
        Off = 0, // stop after the last track in the play sequence
        All,     // loop back to the start when the sequence ends
        One      // replay the current track indefinitely
    };

    PlaylistManager();
    ~PlaylistManager() = default;

    // Playlist management
    void addFile(const juce::File& file);
    void addFiles(const juce::Array<juce::File>& files);
    void addStream(const juce::String& url, const juce::String& name = {});
    void removeItem(int index);
    void clearPlaylist();
    void moveItem(int fromIndex, int toIndex);

    // Access
    const juce::Array<PlaylistItem>& getItems() const { return items; }
    const PlaylistItem* getCurrentItem() const;
    int getCurrentIndex() const { return currentIndex; }

    // Playback
    void selectItem(int index);
    void nextTrack();       // manual navigation — always moves to another track
    void previousTrack();   // manual navigation — always moves to another track

    // Called when the current track finishes playing on its own. Advances to the
    // next track honouring shuffle and repeat mode. With RepeatMode::Off at the end
    // of the play sequence it does nothing (playback should stop). RepeatMode::One
    // is handled by the caller (the same track is replayed), not here.
    void advanceAfterTrackEnd();

    // Shuffle / repeat
    void setShuffle(bool shouldShuffle);
    bool getShuffle() const { return shuffle; }
    void setRepeatMode(RepeatMode mode) { repeatMode = mode; }
    RepeatMode getRepeatMode() const { return repeatMode; }

    // Save/Load
    bool savePlaylist(const juce::File& file);
    bool loadPlaylist(const juce::File& file);

private:
    // Builds a fresh shuffled permutation of [0, items.size()) into shuffleOrder.
    // Does not touch shufflePos.
    void buildShuffleSequence();
    // Rebuilds shuffleOrder and points shufflePos at the current track.
    void rebuildShuffleOrder();
    // Keeps the shuffle sequence valid after the item list changes.
    void refreshShuffleAfterMutation();

    juce::Array<PlaylistItem> items;
    int currentIndex = -1;

    RepeatMode repeatMode = RepeatMode::Off;
    bool shuffle = false;
    juce::Array<int> shuffleOrder; // permutation of item indices defining play order
    int shufflePos = -1;           // position of the current track within shuffleOrder
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaylistManager)
};
