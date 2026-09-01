#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Export/VideoExporter.h"

// Modal settings panel for a video export: mode, combined layout (including a true
// full-frame Overlay blend of grid + visualization), color theme, resolution, and three
// independent optional text bands (top/center/bottom), each with its own static/marquee/
// timed-slide-in-out animation. Replaces the old single AlertWindow (combo boxes and text
// editors only) now that per-text sliders are needed for the animation parameters.
class VideoExportSettingsDialog : public juce::Component
{
public:
    explicit VideoExportSettingsDialog(bool hasPatternData);
    ~VideoExportSettingsDialog() override;

    void resized() override;

    // Reads every control into a fresh Options. The caller still has to fill in
    // sourceFile/isXtpSequence/outputFile/pluginFactory -- this dialog only knows about
    // the render settings, not which file was chosen or which plugin is active.
    VideoExporter::Options buildOptions() const;

    // Invoked when the user clicks Export/Cancel (or dismisses the window). Set these
    // before/after launch() -- they're only ever called later, from a user action.
    std::function<void()> onExport;
    std::function<void()> onCancel;

    // Wraps `dialog` in a modal DialogWindow, shows it centred on screen, and hands
    // ownership to the modal system (self-deletes once dismissed via onExport/onCancel/
    // the window's close button).
    static void launch(std::unique_ptr<VideoExportSettingsDialog> dialog);

private:
    struct TextSlot
    {
        juce::Label label { {}, {} };
        juce::TextEditor editor;
        juce::ComboBox slideCombo;
        juce::Label paramLabel { {}, {} };
        juce::Slider paramSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

        void setUp(juce::Component& parent, const juce::String& labelText);
        void updateParamControl(); // shows/relabels paramSlider for the current slide mode
        VideoExporter::TextLayer buildLayer() const;
        int layout(juce::Rectangle<int> area); // returns bottom Y used
    };

    void updateEnablement();

    bool hasPatternDataFlag;

    juce::Label modeLabel { {}, "Mode" };
    juce::ComboBox modeCombo;
    juce::Label layoutLabel { {}, "Combined layout" };
    juce::ComboBox layoutCombo;
    juce::Label overlayOpacityLabel { {}, "Overlay opacity" };
    juce::Slider overlayOpacitySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label themeLabel { {}, "Color theme" };
    juce::ComboBox themeCombo;
    juce::Label resolutionLabel { {}, "Resolution" };
    juce::ComboBox resolutionCombo;

    TextSlot topSlot, centerSlot, bottomSlot;

    juce::Label tipsLabel { {}, {} };

    juce::TextButton exportButton { "Export" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoExportSettingsDialog)
};
