#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>

class VisualizationPlugin
{
public:
    virtual ~VisualizationPlugin() = default;

    virtual juce::String getName() const = 0;

    // Called on the GUI thread after the audio buffer is locked.
    // Copy whatever data the plugin needs; do not hold references to buffer.
    virtual void update(const juce::AudioBuffer<float>& buffer,
                        const juce::Array<float>& spectrum) = 0;

    virtual void render(juce::Graphics& g, juce::Rectangle<int> bounds) = 0;
};
