#include "VisualizationComponent.h"
#include "../Visualization/Plugins/WaveformPlugin.h"
#include "../Visualization/Plugins/SpectrumBarsPlugin.h"
#include "../Visualization/Plugins/SpectrogramPlugin.h"
#include "../Visualization/Plugins/VUMeterPlugin.h"
#include "../Visualization/Plugins/LissajousPlugin.h"
#include "../Visualization/Plugins/RadialSpectrumPlugin.h"
#include "../Visualization/Plugins/ParticleBloomPlugin.h"
#include "../Visualization/Plugins/BarsPeakHoldPlugin.h"
#include "../Visualization/Plugins/StarfieldTunnelPlugin.h"

VisualizationComponent::VisualizationComponent(AudioEngine* engine)
    : audioEngine(engine)
{
    plugins.add(new WaveformPlugin());
    plugins.add(new SpectrumBarsPlugin());
    plugins.add(new SpectrogramPlugin());
    plugins.add(new VUMeterPlugin());
    plugins.add(new LissajousPlugin());
    plugins.add(new RadialSpectrumPlugin());
    plugins.add(new ParticleBloomPlugin());
    plugins.add(new BarsPeakHoldPlugin());
    plugins.add(new StarfieldTunnelPlugin());

    startTimer(40); // ~25 FPS
}

VisualizationComponent::~VisualizationComponent()
{
    stopTimer();
}

juce::Rectangle<int> VisualizationComponent::pluginArea() const
{
    return getLocalBounds().withTrimmedTop(kTabH);
}

void VisualizationComponent::drawTabs(juce::Graphics& g) const
{
    auto tabBar = getLocalBounds().removeFromTop(kTabH);

    // Dark background for the tab strip
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(tabBar);

    const int n = plugins.size();
    if (n == 0) return;

    const int tabW = tabBar.getWidth() / n;

    for (int i = 0; i < n; ++i)
    {
        auto tab = tabBar.withLeft(i * tabW).withWidth(tabW);

        bool active = (i == activePlugin);
        // Active tab: bright, others: dim
        g.setColour(active ? juce::Colour(0xff2a7abf) : juce::Colour(0xff333333));
        g.fillRect(tab.reduced(1, 0));

        // Tab label
        g.setColour(active ? juce::Colours::white : juce::Colours::grey);
        g.setFont(juce::Font(11.0f, juce::Font::plain));
        g.drawText(plugins[i]->getName(), tab, juce::Justification::centred, true);

        // Separator line between tabs
        g.setColour(juce::Colours::black);
        g.drawVerticalLine(tab.getRight(), (float)tab.getY(), (float)tab.getBottom());
    }

    // Bottom border of tab bar
    g.setColour(juce::Colour(0xff2a7abf));
    g.drawHorizontalLine(tabBar.getBottom() - 1,
                         (float)tabBar.getX(), (float)tabBar.getRight());
}

void VisualizationComponent::timerCallback()
{
    if (!audioEngine->tryLockAudioBuffer())
        return;

    auto& buffer = audioEngine->getAudioBuffer();
    int numSamples = buffer.getNumSamples();

    if (numSamples > 0)
        spectrumAnalyzer.analyze(buffer.getReadPointer(0), numSamples);

    if (activePlugin >= 0 && activePlugin < plugins.size())
        plugins[activePlugin]->update(buffer, spectrumAnalyzer.getSpectrumData());

    audioEngine->unlockAudioBuffer();

    repaint();
}

void VisualizationComponent::paint(juce::Graphics& g)
{
    // Draw the plugin visualizer in the area below the tab bar
    auto area = pluginArea();
    if (activePlugin >= 0 && activePlugin < plugins.size())
        plugins[activePlugin]->render(g, area);
    else
        g.fillRect(area); // black already from fillAll

    // Draw tab bar on top (so it's always visible)
    drawTabs(g);
}

void VisualizationComponent::resized()
{
}

juce::String VisualizationComponent::getPluginName(int index) const
{
    return juce::isPositiveAndBelow(index, plugins.size()) ? plugins[index]->getName() : juce::String();
}

void VisualizationComponent::setActivePluginIndex(int index)
{
    if (juce::isPositiveAndBelow(index, plugins.size()) && index != activePlugin)
    {
        activePlugin = index;
        repaint();
    }
}

bool VisualizationComponent::handleDigitHotkey(const juce::KeyPress& key)
{
    auto c = key.getTextCharacter();
    if (c < '1' || c > '9')
        return false;

    int index = (int)(c - '1');
    if (!juce::isPositiveAndBelow(index, plugins.size()))
        return false;

    setActivePluginIndex(index);
    return true;
}

std::unique_ptr<VisualizationPlugin> VisualizationComponent::createPluginInstance(int index) const
{
    // Mirrors the plugin list built in the constructor above.
    switch (index)
    {
        case 0: return std::make_unique<WaveformPlugin>();
        case 1: return std::make_unique<SpectrumBarsPlugin>();
        case 2: return std::make_unique<SpectrogramPlugin>();
        case 3: return std::make_unique<VUMeterPlugin>();
        case 4: return std::make_unique<LissajousPlugin>();
        case 5: return std::make_unique<RadialSpectrumPlugin>();
        case 6: return std::make_unique<ParticleBloomPlugin>();
        case 7: return std::make_unique<BarsPeakHoldPlugin>();
        case 8: return std::make_unique<StarfieldTunnelPlugin>();
        default: return nullptr;
    }
}

void VisualizationComponent::mouseDown(const juce::MouseEvent& e)
{
    // Check if click is in the tab bar
    if (e.y >= kTabH || plugins.size() == 0)
        return;

    int tabW = getWidth() / plugins.size();
    if (tabW <= 0) return;

    int clicked = e.x / tabW;
    clicked = juce::jlimit(0, plugins.size() - 1, clicked);
    if (clicked != activePlugin)
    {
        activePlugin = clicked;
        repaint();
    }
}
