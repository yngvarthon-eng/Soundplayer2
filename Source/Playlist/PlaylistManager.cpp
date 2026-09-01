#include "PlaylistManager.h"
#include "../Xtp/XtpSong.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
    juce::String getFirstMetadataValue(const juce::StringPairArray& metadata,
                                       std::initializer_list<const char*> keys)
    {
        for (auto* key : keys)
        {
            auto value = metadata.getValue(key, {});
            if (value.isNotEmpty())
                return value;
        }

        return {};
    }

    juce::String describeChannelLayout(int channelCount)
    {
        switch (channelCount)
        {
            case 1: return "Mono";
            case 2: return "Stereo";
            case 3: return "3.0";
            case 4: return "Quad";
            case 5: return "5.0";
            case 6: return "5.1";
            case 7: return "6.1";
            case 8: return "7.1";
            default: break;
        }

        return channelCount > 0 ? juce::String(channelCount) + " ch" : juce::String();
    }

    void populateXtpMetadata(PlaylistItem& item)
    {
        xtp::Song song;
        juce::String error;
        if (!song.loadFromFile(item.file, error))
            return;

        item.duration = song.estimateDurationSeconds();
        item.channelLayout = juce::String(song.channels) + " ch";
        if (song.moduleMessage.isNotEmpty())
            item.artist = song.moduleMessage;
    }

    void populateFileMetadata(PlaylistItem& item)
    {
        if (item.isXtpSequence)
        {
            populateXtpMetadata(item);
            return;
        }

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        auto* reader = formatManager.createReaderFor(item.file);
        if (reader == nullptr)
            return;

        std::unique_ptr<juce::AudioFormatReader> ownedReader(reader);
        if (reader->sampleRate > 0.0)
            item.duration = (double) reader->lengthInSamples / reader->sampleRate;

        item.artist = getFirstMetadataValue(reader->metadataValues, { "artist", "Author", "author" });
        item.album = getFirstMetadataValue(reader->metadataValues, { "album", "Album" });
        item.channelLayout = describeChannelLayout((int) reader->numChannels);
    }
}

PlaylistManager::PlaylistManager()
{
}

void PlaylistManager::addFile(const juce::File& file)
{
    if (file.exists())
    {
        PlaylistItem item(file);
        populateFileMetadata(item);
        items.add(item);
        refreshShuffleAfterMutation();
        sendChangeMessage();
    }
}

void PlaylistManager::addFiles(const juce::Array<juce::File>& files)
{
    for (const auto& file : files)
    {
        if (file.exists())
        {
            PlaylistItem item(file);
            populateFileMetadata(item);
            items.add(item);
        }
    }
    refreshShuffleAfterMutation();
    sendChangeMessage();
}

void PlaylistManager::addStream(const juce::String& url, const juce::String& name)
{
    if (url.isNotEmpty())
    {
        items.add(PlaylistItem(url, name.isEmpty() ? url : name));
        refreshShuffleAfterMutation();
        sendChangeMessage();
    }
}

void PlaylistManager::removeItem(int index)
{
    if (juce::isPositiveAndBelow(index, items.size()))
    {
        items.remove(index);
        if (currentIndex >= items.size() && currentIndex > 0)
            currentIndex--;
        refreshShuffleAfterMutation();
        sendChangeMessage();
    }
}

void PlaylistManager::clearPlaylist()
{
    items.clear();
    currentIndex = -1;
    shuffleOrder.clearQuick();
    shufflePos = -1;
    sendChangeMessage();
}

void PlaylistManager::moveItem(int fromIndex, int toIndex)
{
    if (juce::isPositiveAndBelow(fromIndex, items.size()) && juce::isPositiveAndBelow(toIndex, items.size()))
    {
        auto item = items[fromIndex];
        items.remove(fromIndex);
        items.insert(toIndex, item);
        refreshShuffleAfterMutation();
        sendChangeMessage();
    }
}

const PlaylistItem* PlaylistManager::getCurrentItem() const
{
    if (juce::isPositiveAndBelow(currentIndex, items.size()))
        return &items.getReference(currentIndex);
    return nullptr;
}

void PlaylistManager::selectItem(int index)
{
    if (juce::isPositiveAndBelow(index, items.size()))
    {
        currentIndex = index;
        if (shuffle)
        {
            if (shuffleOrder.size() != items.size())
                rebuildShuffleOrder();
            shufflePos = shuffleOrder.indexOf(currentIndex);
        }
        sendChangeMessage();
    }
}

void PlaylistManager::nextTrack()
{
    if (items.isEmpty())
        return;

    if (shuffle)
    {
        if (shuffleOrder.size() != items.size())
            rebuildShuffleOrder();

        if (shufflePos + 1 >= shuffleOrder.size())
        {
            // Reached the end of the current shuffle cycle — reshuffle for a new one.
            buildShuffleSequence();
            // Avoid immediately repeating the track that just played.
            if (shuffleOrder.size() > 1 && shuffleOrder[0] == currentIndex)
                shuffleOrder.swap(0, 1);
            shufflePos = 0;
        }
        else
        {
            ++shufflePos;
        }
        currentIndex = shuffleOrder[shufflePos];
    }
    else
    {
        currentIndex = (currentIndex + 1) % items.size();
    }

    sendChangeMessage();
}

void PlaylistManager::previousTrack()
{
    if (items.isEmpty())
        return;

    if (shuffle)
    {
        if (shuffleOrder.size() != items.size())
            rebuildShuffleOrder();

        --shufflePos;
        if (shufflePos < 0)
            shufflePos = shuffleOrder.size() - 1; // wrap to the end of the sequence
        currentIndex = shuffleOrder[shufflePos];
    }
    else
    {
        currentIndex = (currentIndex - 1 + items.size()) % items.size();
    }

    sendChangeMessage();
}

void PlaylistManager::advanceAfterTrackEnd()
{
    if (items.isEmpty())
        return;

    if (shuffle && shuffleOrder.size() != items.size())
        rebuildShuffleOrder();

    const bool atEndOfSequence = shuffle ? (shufflePos >= shuffleOrder.size() - 1)
                                         : (currentIndex >= items.size() - 1);

    // With repeat off, stop once the play sequence is exhausted.
    if (repeatMode == RepeatMode::Off && atEndOfSequence)
        return;

    nextTrack();
}

void PlaylistManager::setShuffle(bool shouldShuffle)
{
    if (shuffle == shouldShuffle)
        return;

    shuffle = shouldShuffle;
    if (shuffle)
        rebuildShuffleOrder();
    else
        shufflePos = -1;
}

void PlaylistManager::buildShuffleSequence()
{
    shuffleOrder.clearQuick();
    for (int i = 0; i < items.size(); ++i)
        shuffleOrder.add(i);

    // Fisher-Yates shuffle.
    for (int i = shuffleOrder.size() - 1; i > 0; --i)
        shuffleOrder.swap(i, random.nextInt(i + 1));
}

void PlaylistManager::rebuildShuffleOrder()
{
    buildShuffleSequence();
    shufflePos = shuffleOrder.indexOf(currentIndex); // -1 when nothing is selected
}

void PlaylistManager::refreshShuffleAfterMutation()
{
    if (shuffle)
        rebuildShuffleOrder();
}

bool PlaylistManager::savePlaylist(const juce::File& file)
{
    juce::XmlElement playlist("PLAYLIST");

    for (const auto& item : items)
    {
        auto itemElem = playlist.createNewChildElement("ITEM");
        itemElem->setAttribute("name", item.name);
        itemElem->setAttribute("duration", item.duration);
        itemElem->setAttribute("artist", item.artist);
        itemElem->setAttribute("album", item.album);
        itemElem->setAttribute("channelLayout", item.channelLayout);
        if (item.isStream)
            itemElem->setAttribute("streamUrl", item.streamUrl);
        else
            itemElem->setAttribute("file", item.file.getFullPathName());
    }

    return playlist.writeTo(file);
}

bool PlaylistManager::loadPlaylist(const juce::File& file)
{
    if (!file.exists())
        return false;

    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return false;

    items.clear();
    currentIndex = -1;
    shuffleOrder.clearQuick();
    shufflePos = -1;

    for (auto* itemElem : xml->getChildIterator())
    {
        auto streamUrl = itemElem->getStringAttribute("streamUrl");
        if (streamUrl.isNotEmpty())
        {
            PlaylistItem item(streamUrl, itemElem->getStringAttribute("name"));
            item.channelLayout = itemElem->getStringAttribute("channelLayout");
            items.add(item);
        }
        else
        {
            juce::File audioFile(itemElem->getStringAttribute("file"));
            if (audioFile.exists())
            {
                PlaylistItem item(audioFile);
                item.name     = itemElem->getStringAttribute("name", item.name);
                item.duration = itemElem->getDoubleAttribute("duration", 0.0);
                item.artist   = itemElem->getStringAttribute("artist");
                item.album    = itemElem->getStringAttribute("album");
                item.channelLayout = itemElem->getStringAttribute("channelLayout");
                if (item.channelLayout.isEmpty())
                    populateFileMetadata(item);
                items.add(item);
            }
        }
    }

    refreshShuffleAfterMutation();
    sendChangeMessage();
    return true;
}
