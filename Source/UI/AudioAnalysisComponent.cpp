#include "AudioAnalysisComponent.h"
#include "../Audio/AudioEngine.h"

namespace
{
    juce::String formatSampleRate(double sampleRate)
    {
        if (sampleRate <= 0.0)
            return {};

        if (std::fmod(sampleRate, 1000.0) == 0.0)
            return juce::String((int) (sampleRate / 1000.0)) + " kHz";

        return juce::String(sampleRate / 1000.0, 1) + " kHz";
    }
}

AudioAnalysisComponent::AudioAnalysisComponent()
{
    formatManager.registerBasicFormats();

    analyzeBtn.onClick = [this]
    {
        if (currentFile.existsAsFile())
            analyzeFile(currentFile);
    };
    addAndMakeVisible(analyzeBtn);
}

void AudioAnalysisComponent::analyzeFile(const juce::File& file)
{
    currentFile = file;
    analyzing   = true;
    lastResult  = {};
    sourceInfo  = {};

    if (auto* reader = formatManager.createReaderFor(file))
    {
        sourceInfo = AudioEngine::describeChannelLayout((int) reader->numChannels);

        auto sampleRateText = formatSampleRate(reader->sampleRate);
        if (sampleRateText.isNotEmpty())
            sourceInfo += "  " + sampleRateText;

        delete reader;
    }

    repaint();

    analyzer.analyzeFile(file, formatManager, [this](AudioAnalyzer::Result r)
    {
        analyzing  = false;
        lastResult = r;
        repaint();
    });
}

void AudioAnalysisComponent::clear()
{
    analyzer.stopThread(300);
    analyzing   = false;
    lastResult  = {};
    currentFile = juce::File();
    sourceInfo  = {};
    repaint();
}

void AudioAnalysisComponent::resized()
{
    auto area = getLocalBounds().reduced(4);
    analyzeBtn.setBounds(area.removeFromRight(74).withSizeKeepingCentre(70, 22));
}

void AudioAnalysisComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff252525));
    g.setColour(juce::Colour(0xff444444));
    g.drawRect(getLocalBounds(), 1);

    auto area = getLocalBounds().reduced(6);
    area.removeFromRight(82); // leave room for button

    auto headerArea = area.removeFromTop(14);

    // Label
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff888888));
    g.drawText("ANALYSIS", headerArea.removeFromLeft(64), juce::Justification::centredLeft);

    if (sourceInfo.isNotEmpty())
    {
        g.setFont(juce::Font(11.0f));
        g.setColour(juce::Colour(0xffaab7c5));
        g.drawText(sourceInfo, headerArea, juce::Justification::centredLeft);
    }

    area.removeFromTop(2);

    if (analyzing)
    {
        g.setFont(juce::Font(12.0f));
        g.setColour(juce::Colours::yellow);
        g.drawText("Analyzing...", area, juce::Justification::centredLeft);
        return;
    }

    if (!lastResult.valid)
    {
        g.setFont(juce::Font(12.0f));
        g.setColour(juce::Colour(0xff555555));
        g.drawText("No data — press Analyze", area, juce::Justification::centredLeft);
        return;
    }

    // --- BPM badge ---
    auto bpmArea = area.removeFromLeft(110);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff88ddff));
    juce::String bpmStr = lastResult.bpm > 0.0f
                          ? juce::String((int)std::round(lastResult.bpm))
                          : "--";
    g.drawText(bpmStr, bpmArea.removeFromLeft(52), juce::Justification::centredRight);
    g.setFont(juce::Font(11.0f));
    g.setColour(juce::Colour(0xff888888));
    g.drawText("BPM", bpmArea, juce::Justification::centredLeft);

    // --- RMS + Peak ---
    auto statsArea = area;
    auto rmsRow  = statsArea.removeFromTop(statsArea.getHeight() / 2);
    auto peakRow = statsArea;

    g.setFont(juce::Font(11.0f));
    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawText("RMS", rmsRow.removeFromLeft(32), juce::Justification::centredLeft);
    g.setColour(juce::Colours::lightgreen);
    g.drawText(juce::String(lastResult.rmsDb, 1) + " dBFS", rmsRow, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawText("Peak", peakRow.removeFromLeft(32), juce::Justification::centredLeft);

    // Colour-code the peak: red if clipping, orange if hot, green otherwise
    float pk = lastResult.peakDb;
    juce::Colour pkColour = pk >= 0.0f    ? juce::Colours::red
                          : pk >= -3.0f  ? juce::Colours::orange
                                         : juce::Colours::lightgreen;
    g.setColour(pkColour);
    g.drawText(juce::String(pk, 1) + " dBFS", peakRow, juce::Justification::centredLeft);
}
