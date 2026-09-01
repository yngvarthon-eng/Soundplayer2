#include "VideoExportSettingsDialog.h"

namespace
{
    constexpr int kSlotHeight = 88;

    // Own DialogWindow subclass rather than DialogWindow::LaunchOptions -- the default
    // LaunchOptions-created window's closeButtonPressed() just calls setVisible(false)
    // rather than exitModalState(), which would leave the window hidden but never torn
    // down (and deleteWhenDismissed never firing) when closed via the title bar / Escape.
    class SettingsDialogWindow : public juce::DialogWindow
    {
    public:
        SettingsDialogWindow()
            : DialogWindow("Export Video Settings", juce::Colour(0xff2a2a2a), true, true)
        {
        }

        void closeButtonPressed() override { exitModalState(0); }
    };
}

void VideoExportSettingsDialog::TextSlot::setUp(juce::Component& parent, const juce::String& labelText)
{
    label.setText(labelText, juce::dontSendNotification);
    parent.addAndMakeVisible(label);

    editor.setTextToShowWhenEmpty("(optional)", juce::Colours::grey);
    parent.addAndMakeVisible(editor);

    slideCombo.addItem("Static", 1);
    slideCombo.addItem("Marquee (scroll)", 2);
    slideCombo.addItem("Slide in/out (timed)", 3);
    slideCombo.setSelectedId(1, juce::dontSendNotification);
    slideCombo.onChange = [this] { updateParamControl(); };
    parent.addAndMakeVisible(slideCombo);

    parent.addAndMakeVisible(paramLabel);
    parent.addAndMakeVisible(paramSlider);

    updateParamControl();
}

void VideoExportSettingsDialog::TextSlot::updateParamControl()
{
    switch (slideCombo.getSelectedItemIndex())
    {
        case 1: // Marquee
            paramLabel.setText("Crossing time (s)", juce::dontSendNotification);
            paramSlider.setRange(1.0, 20.0, 0.5);
            paramSlider.setValue(8.0, juce::dontSendNotification);
            paramLabel.setEnabled(true);
            paramSlider.setEnabled(true);
            break;
        case 2: // Timed slide in/out
            paramLabel.setText("Hold time (s)", juce::dontSendNotification);
            paramSlider.setRange(0.5, 10.0, 0.5);
            paramSlider.setValue(3.0, juce::dontSendNotification);
            paramLabel.setEnabled(true);
            paramSlider.setEnabled(true);
            break;
        default: // Static
            paramLabel.setText("(not animated)", juce::dontSendNotification);
            paramLabel.setEnabled(false);
            paramSlider.setEnabled(false);
            break;
    }
}

VideoExporter::TextLayer VideoExportSettingsDialog::TextSlot::buildLayer() const
{
    VideoExporter::TextLayer layer;
    layer.text = editor.getText();

    static const VideoExporter::TextSlide slides[] = {
        VideoExporter::TextSlide::Static,
        VideoExporter::TextSlide::Marquee,
        VideoExporter::TextSlide::TimedSlideInOut
    };
    layer.slide = slides[juce::jlimit(0, 2, slideCombo.getSelectedItemIndex())];

    if (layer.slide == VideoExporter::TextSlide::Marquee)
        layer.marqueeSeconds = paramSlider.getValue();
    else if (layer.slide == VideoExporter::TextSlide::TimedSlideInOut)
        layer.holdSeconds = paramSlider.getValue();

    return layer;
}

int VideoExportSettingsDialog::TextSlot::layout(juce::Rectangle<int> area)
{
    label.setBounds(area.removeFromTop(20));

    editor.setBounds(area.removeFromTop(26));
    area.removeFromTop(4);

    auto paramRow = area.removeFromTop(24);
    slideCombo.setBounds(paramRow.removeFromLeft(160));
    paramRow.removeFromLeft(8);
    paramLabel.setBounds(paramRow.removeFromLeft(110));
    paramSlider.setBounds(paramRow);

    return area.getY();
}

VideoExportSettingsDialog::VideoExportSettingsDialog(bool hasPatternData)
    : hasPatternDataFlag(hasPatternData)
{
    addAndMakeVisible(modeLabel);
    modeCombo.addItem("Visualization", 1);
    if (hasPatternDataFlag)
    {
        modeCombo.addItem("Pattern Grid", 2);
        modeCombo.addItem("Combined", 3);
    }
    modeCombo.setSelectedId(1, juce::dontSendNotification);
    modeCombo.onChange = [this] { updateEnablement(); };
    addAndMakeVisible(modeCombo);

    addAndMakeVisible(layoutLabel);
    layoutCombo.addItem("Split (grid top)", 1);
    layoutCombo.addItem("Split (viz top)", 2);
    layoutCombo.addItem("Picture-in-picture (viz over grid)", 3);
    layoutCombo.addItem("Picture-in-picture (grid over viz)", 4);
    layoutCombo.addItem("Overlay (viz blended over grid)", 5);
    layoutCombo.setSelectedId(1, juce::dontSendNotification);
    layoutCombo.onChange = [this] { updateEnablement(); };
    addAndMakeVisible(layoutCombo);

    addAndMakeVisible(overlayOpacityLabel);
    overlayOpacitySlider.setRange(0.1, 0.9, 0.05);
    overlayOpacitySlider.setValue(0.55, juce::dontSendNotification);
    addAndMakeVisible(overlayOpacitySlider);

    addAndMakeVisible(themeLabel);
    themeCombo.addItem("Classic", 1);
    themeCombo.addItem("Mono Green", 2);
    themeCombo.addItem("Mono Amber", 3);
    themeCombo.addItem("Inverted", 4);
    themeCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(themeCombo);

    addAndMakeVisible(resolutionLabel);
    resolutionCombo.addItem("1280 x 720", 1);
    resolutionCombo.addItem("1920 x 1080", 2);
    resolutionCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(resolutionCombo);

    topSlot.setUp(*this, "Top text");
    centerSlot.setUp(*this, "Center text");
    bottomSlot.setUp(*this, "Bottom text");

    tipsLabel.setText(
        "Tips: Overlay reads best with a busy grid + a sparse plugin (Bars/Radial/VU) -- "
        "dense ones like Spectrogram fight the grid for attention. Give Marquee text a few "
        "seconds' crossing time so a full title stays readable. Timed slide-in suits a short "
        "caption (song/artist), not a wall of text.",
        juce::dontSendNotification);
    tipsLabel.setFont(juce::Font(12.0f));
    tipsLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    tipsLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(tipsLabel);

    exportButton.onClick = [this] { if (onExport) onExport(); };
    cancelButton.onClick = [this] { if (onCancel) onCancel(); };
    addAndMakeVisible(exportButton);
    addAndMakeVisible(cancelButton);

    updateEnablement();
    setSize(480, 610);
}

VideoExportSettingsDialog::~VideoExportSettingsDialog() = default;

void VideoExportSettingsDialog::updateEnablement()
{
    const bool combined = hasPatternDataFlag && modeCombo.getSelectedItemIndex() == 2;
    layoutLabel.setEnabled(combined);
    layoutCombo.setEnabled(combined);

    const bool overlay = combined && layoutCombo.getSelectedItemIndex() == 4;
    overlayOpacityLabel.setEnabled(overlay);
    overlayOpacitySlider.setEnabled(overlay);
}

void VideoExportSettingsDialog::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto row = area.removeFromTop(22);
    modeLabel.setBounds(row.removeFromLeft(140));
    modeCombo.setBounds(row);
    area.removeFromTop(8);

    row = area.removeFromTop(22);
    layoutLabel.setBounds(row.removeFromLeft(140));
    layoutCombo.setBounds(row);
    area.removeFromTop(8);

    row = area.removeFromTop(22);
    overlayOpacityLabel.setBounds(row.removeFromLeft(140));
    overlayOpacitySlider.setBounds(row);
    area.removeFromTop(8);

    row = area.removeFromTop(22);
    themeLabel.setBounds(row.removeFromLeft(140));
    themeCombo.setBounds(row);
    area.removeFromTop(8);

    row = area.removeFromTop(22);
    resolutionLabel.setBounds(row.removeFromLeft(140));
    resolutionCombo.setBounds(row);
    area.removeFromTop(16);

    topSlot.layout(area.removeFromTop(kSlotHeight));
    area.removeFromTop(8);
    centerSlot.layout(area.removeFromTop(kSlotHeight));
    area.removeFromTop(8);
    bottomSlot.layout(area.removeFromTop(kSlotHeight));
    area.removeFromTop(12);

    tipsLabel.setBounds(area.removeFromTop(64));
    area.removeFromTop(12);

    row = area.removeFromTop(28);
    cancelButton.setBounds(row.removeFromRight(90));
    row.removeFromRight(8);
    exportButton.setBounds(row.removeFromRight(90));
}

VideoExporter::Options VideoExportSettingsDialog::buildOptions() const
{
    VideoExporter::Options options;

    static const VideoExporter::ExportMode modes[] = {
        VideoExporter::ExportMode::Visualization,
        VideoExporter::ExportMode::PatternGrid,
        VideoExporter::ExportMode::Combined
    };
    options.mode = modes[juce::jlimit(0, 2, modeCombo.getSelectedItemIndex())];

    static const VideoExporter::CombinedLayout layouts[] = {
        VideoExporter::CombinedLayout::SplitGridTop,
        VideoExporter::CombinedLayout::SplitVizTop,
        VideoExporter::CombinedLayout::PipViz,
        VideoExporter::CombinedLayout::PipGrid,
        VideoExporter::CombinedLayout::Overlay
    };
    options.combinedLayout = layouts[juce::jlimit(0, 4, layoutCombo.getSelectedItemIndex())];
    options.combinedOverlayOpacity = (float) overlayOpacitySlider.getValue();

    static const VideoExporter::ColorTheme themes[] = {
        VideoExporter::ColorTheme::Classic,
        VideoExporter::ColorTheme::MonoGreen,
        VideoExporter::ColorTheme::MonoAmber,
        VideoExporter::ColorTheme::Inverted
    };
    options.colorTheme = themes[juce::jlimit(0, 3, themeCombo.getSelectedItemIndex())];

    const int resIdx = resolutionCombo.getSelectedItemIndex();
    options.width  = resIdx == 0 ? 1280 : 1920;
    options.height = resIdx == 0 ? 720  : 1080;

    options.topText = topSlot.buildLayer();
    options.centerText = centerSlot.buildLayer();
    options.bottomText = bottomSlot.buildLayer();

    return options;
}

void VideoExportSettingsDialog::launch(std::unique_ptr<VideoExportSettingsDialog> dialog)
{
    auto* dialogPtr = dialog.get();
    auto* window = new SettingsDialogWindow();
    window->setContentOwned(dialog.release(), true);
    window->setResizable(false, false);
    window->setUsingNativeTitleBar(true);
    window->centreWithSize(window->getWidth(), window->getHeight());

    // Wrap whatever the caller already set: the window has to close either way, but the
    // caller's own onExport/onCancel (set before calling launch()) still needs to run first
    // so it can read buildOptions() while the dialog's controls are still alive.
    auto userOnExport = std::move(dialogPtr->onExport);
    auto userOnCancel = std::move(dialogPtr->onCancel);

    dialogPtr->onExport = [window, userOnExport]
    {
        if (userOnExport)
            userOnExport();
        window->exitModalState(1);
    };
    dialogPtr->onCancel = [window, userOnCancel]
    {
        if (userOnCancel)
            userOnCancel();
        window->exitModalState(0);
    };

    window->setVisible(true);
    window->enterModalState(true, nullptr, true);
}
