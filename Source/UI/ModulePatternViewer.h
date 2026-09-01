#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Visualization/ModulePatternData.h"

class PatternGridComponent;

class ModulePatternViewer : public juce::Component
{
public:
    ModulePatternViewer();
    ~ModulePatternViewer() override;

    void setPatternData(const ModulePatternData* data);
    void setCurrentRow(int row);

    // When enabled, the grid shrinks each channel column so every channel in
    // the pattern fits within the viewport width instead of being clipped by
    // it -- there's no scrollbar in a baked video frame, so used by
    // VideoExporter's PatternGrid/Combined modes. Off by default, which keeps
    // the interactive viewer's fixed-width, horizontally-scrollable columns.
    void setFitChannelsToWidth(bool shouldFit);

    // Call periodically to auto-follow playback (switches pattern and highlights row)
    void setPlaybackPosition(int patternIndex, int row);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void showPattern(int index);
    void updateNavButtons();
    void refitGrid();

    const ModulePatternData* patternData = nullptr;
    int currentPatternIndex = 0;
    bool fitChannelsToWidth = false;

    std::unique_ptr<juce::Label>      titleLabel;
    std::unique_ptr<juce::Label>      patternLabel;
    std::unique_ptr<juce::TextButton> prevButton;
    std::unique_ptr<juce::TextButton> nextButton;
    std::unique_ptr<juce::Label>      statusLabel;
    std::unique_ptr<juce::Viewport>   viewport;
    std::unique_ptr<PatternGridComponent> grid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulePatternViewer)
};
