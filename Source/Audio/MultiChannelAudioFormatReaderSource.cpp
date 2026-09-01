#include "MultiChannelAudioFormatReaderSource.h"

MultiChannelAudioFormatReaderSource::MultiChannelAudioFormatReaderSource(
    juce::AudioFormatReader* sourceReader,
    bool deleteReaderWhenThisIsDeleted)
    : reader(sourceReader, deleteReaderWhenThisIsDeleted)
{
    jassert(reader != nullptr);
}

MultiChannelAudioFormatReaderSource::~MultiChannelAudioFormatReaderSource() = default;

void MultiChannelAudioFormatReaderSource::setLooping(bool shouldLoop)
{
    looping = shouldLoop;
}

void MultiChannelAudioFormatReaderSource::prepareToPlay(int, double)
{
}

void MultiChannelAudioFormatReaderSource::releaseResources()
{
}

void MultiChannelAudioFormatReaderSource::setNextReadPosition(juce::int64 newPosition)
{
    nextPlayPos = newPosition;
}

juce::int64 MultiChannelAudioFormatReaderSource::getNextReadPosition() const
{
    return looping ? nextPlayPos % reader->lengthInSamples : nextPlayPos;
}

juce::int64 MultiChannelAudioFormatReaderSource::getTotalLength() const
{
    return reader->lengthInSamples;
}

void MultiChannelAudioFormatReaderSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (info.numSamples <= 0)
        return;

    const auto start = nextPlayPos;

    auto readIntoBuffer = [this, &info](juce::int64 readerStartSample, int destOffset, int samplesToRead)
    {
        juce::HeapBlock<float*> channels(info.buffer->getNumChannels());
        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
            channels[ch] = info.buffer->getWritePointer(ch, info.startSample + destOffset);

        reader->read(channels.getData(), info.buffer->getNumChannels(), readerStartSample, samplesToRead);
    };

    if (looping)
    {
        const auto newStart = start % reader->lengthInSamples;
        const auto newEnd = (start + info.numSamples) % reader->lengthInSamples;

        if (newEnd > newStart)
        {
            readIntoBuffer(newStart, 0, (int) (newEnd - newStart));
        }
        else
        {
            const int endSamples = (int) (reader->lengthInSamples - newStart);
            readIntoBuffer(newStart, 0, endSamples);
            readIntoBuffer(0, endSamples, (int) newEnd);
        }

        nextPlayPos = newEnd;
        return;
    }

    const auto samplesToRead = juce::jlimit((juce::int64) 0,
                                            (juce::int64) info.numSamples,
                                            reader->lengthInSamples - start);

    if (samplesToRead > 0)
        readIntoBuffer(start, 0, (int) samplesToRead);

    if (samplesToRead < info.numSamples)
        info.buffer->clear(info.startSample + (int) samplesToRead,
                           info.numSamples - (int) samplesToRead);

    nextPlayPos += info.numSamples;
}