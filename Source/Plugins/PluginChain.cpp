#include "PluginChain.h"

void PluginChain::prepare(double sampleRate, int maxBlockSize)
{
    juce::ScopedLock sl(lock);
    currentSampleRate = sampleRate;
    currentBlockSize  = maxBlockSize;

    for (auto& e : plugins)
        e.instance->prepareToPlay(sampleRate, maxBlockSize);
}

void PluginChain::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedTryLock stl(lock);
    if (!stl.isLocked())
        return; // drop the frame rather than block the audio thread

    juce::MidiBuffer midi;
    for (auto& e : plugins)
    {
        if (!e.bypassed)
            e.instance->processBlock(buffer, midi);
    }
}

void PluginChain::addPlugin(std::unique_ptr<juce::AudioPluginInstance> instance)
{
    instance->prepareToPlay(currentSampleRate, currentBlockSize);
    juce::ScopedLock sl(lock);
    plugins.push_back({ std::move(instance), false });
}

void PluginChain::removePlugin(int index)
{
    juce::ScopedLock sl(lock);
    if (index >= 0 && index < (int)plugins.size())
    {
        plugins[index].instance->releaseResources();
        plugins.erase(plugins.begin() + index);
    }
}

void PluginChain::movePlugin(int fromIndex, int toIndex)
{
    juce::ScopedLock sl(lock);
    if (fromIndex < 0 || fromIndex >= (int)plugins.size())
        return;
    toIndex = juce::jlimit(0, (int)plugins.size() - 1, toIndex);
    if (fromIndex == toIndex)
        return;

    auto entry = std::move(plugins[fromIndex]);
    plugins.erase(plugins.begin() + fromIndex);
    plugins.insert(plugins.begin() + toIndex, std::move(entry));
}

void PluginChain::setBypass(int index, bool bypassed)
{
    juce::ScopedLock sl(lock);
    if (index >= 0 && index < (int)plugins.size())
        plugins[index].bypassed = bypassed;
}

int PluginChain::size() const
{
    juce::ScopedLock sl(lock);
    return (int)plugins.size();
}

juce::AudioPluginInstance* PluginChain::getPlugin(int index) const
{
    juce::ScopedLock sl(lock);
    if (index >= 0 && index < (int)plugins.size())
        return plugins[index].instance.get();
    return nullptr;
}

bool PluginChain::isBypassed(int index) const
{
    juce::ScopedLock sl(lock);
    if (index >= 0 && index < (int)plugins.size())
        return plugins[index].bypassed;
    return false;
}

juce::String PluginChain::getPluginName(int index) const
{
    juce::ScopedLock sl(lock);
    if (index >= 0 && index < (int)plugins.size())
        return plugins[index].instance->getName();
    return {};
}
