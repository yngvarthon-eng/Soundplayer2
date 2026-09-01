#include "ModulePatternViewer.h"

// ---- PatternGridComponent ----
// Renders the tracker-style pattern grid.

class PatternGridComponent : public juce::Component
{
public:
    static constexpr int rowHeight           = 14;
    static constexpr int headerHeight        = 18;
    static constexpr int rowNumWidth         = 38;
    static constexpr int defaultChannelWidth = 100;
    static constexpr int minChannelWidth     = 10;

    // Column-width tiers the cell/header painters switch on -- see paintCell().
    static constexpr int fullCellMinWidth    = 70; // note+inst+vol+effect
    static constexpr int compactCellMinWidth = 30; // note+inst only
    static constexpr int minimalCellMinWidth = 14; // note only, tiny font
    // below minimalCellMinWidth: a bare colour swatch, no text at all

    PatternGridComponent()
    {
        setSize(rowNumWidth, headerHeight + 10);
    }

    // fitWidth <= 0: fixed defaultChannelWidth columns, scrolled interactively
    // (the on-screen pattern viewer). fitWidth > 0: shrink every column so all
    // of the pattern's channels fit within fitWidth with no horizontal
    // scrolling/clipping -- used for baked video export, which renders one
    // static frame per row and has no scrollbar. Never grows a column past
    // defaultChannelWidth.
    void setPattern(const ModulePattern* pat, int patIndex, int fitWidth = -1)
    {
        pattern      = pat;
        patternIndex = patIndex;

        if (pat != nullptr && pat->numChannels > 0)
        {
            channelWidth = fitWidth > 0
                ? juce::jlimit(minChannelWidth, defaultChannelWidth,
                                (fitWidth - rowNumWidth) / pat->numChannels)
                : defaultChannelWidth;
            setSize(rowNumWidth + pat->numChannels * channelWidth,
                    headerHeight + pat->numRows * rowHeight);
        }
        else
        {
            channelWidth = defaultChannelWidth;
            setSize(rowNumWidth, headerHeight + 10);
        }

        repaint();
    }

    void setCurrentRow(int row)
    {
        if (currentRow == row) return;
        currentRow = row;
        repaint();
        scrollToRow(row);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(bgColor);

        if (pattern == nullptr)
            return;

        auto clip = g.getClipBounds();

        if (clip.getY() < headerHeight)
            paintHeader(g);

        int firstRow = juce::jmax(0, (clip.getY() - headerHeight) / rowHeight);
        int lastRow  = juce::jmin(pattern->numRows - 1,
                                  (clip.getBottom() - headerHeight) / rowHeight + 1);

        for (int r = firstRow; r <= lastRow; ++r)
            paintRow(g, r);
    }

private:
    void paintHeader(juce::Graphics& g)
    {
        g.setColour(headerBgColor);
        g.fillRect(0, 0, getWidth(), headerHeight);

        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::bold));
        g.setColour(headerTextColor);

        g.drawText("Row", 2, 0, rowNumWidth - 4, headerHeight, juce::Justification::centredLeft);

        if (pattern != nullptr)
        {
            for (int c = 0; c < pattern->numChannels; ++c)
            {
                int x = rowNumWidth + c * channelWidth;

                if (channelWidth >= 44)
                    g.drawText("Ch " + juce::String(c + 1), x + 4, 0, channelWidth - 8,
                               headerHeight, juce::Justification::centredLeft);
                else if (channelWidth >= 16)
                    g.drawText(juce::String(c + 1), x + 2, 0, channelWidth - 4,
                               headerHeight, juce::Justification::centred);

                g.setColour(separatorColor);
                g.drawVerticalLine(x - 1, 0.0f, (float)headerHeight);
                g.setColour(headerTextColor);
            }
        }

        g.setColour(separatorColor.withAlpha(0.8f));
        g.drawHorizontalLine(headerHeight - 1, 0.0f, (float)getWidth());
    }

    void paintRow(juce::Graphics& g, int row)
    {
        if (pattern == nullptr) return;

        int  y         = headerHeight + row * rowHeight;
        bool isCurrent = (row == currentRow);
        bool isBeat    = (row % 4 == 0);

        // Background
        if (isCurrent)
            g.setColour(currentRowBg);
        else if (isBeat)
            g.setColour(beatRowBg);
        else
            g.setColour((row % 2 == 0) ? rowEvenBg : rowOddBg);
        g.fillRect(0, y, getWidth(), rowHeight);

        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::plain));

        // Row number
        g.setColour(isCurrent ? juce::Colours::white : rowNumColor);
        g.drawText(juce::String(row).paddedLeft('0', 3), 2, y, rowNumWidth - 4,
                   rowHeight, juce::Justification::centredLeft);

        // Channel cells
        for (int c = 0; c < pattern->numChannels; ++c)
        {
            int x = rowNumWidth + c * channelWidth;

            g.setColour(separatorColor);
            g.drawVerticalLine(x - 1, (float)y, (float)(y + rowHeight));

            paintCell(g, pattern->at(row, c), x, y);
        }
    }

    // Column width decides how much detail a cell can show -- see the width
    // tiers declared above. High channel counts (32/64ch) shrink columns past
    // the point where all four fields are legible, so cells progressively drop
    // the effect, then volume, then instrument column rather than clipping text.
    void paintCell(juce::Graphics& g, const ModuleNote& note, int x, int y)
    {
        auto noteStr = note.getNoteString();
        bool emptyNote = (noteStr == "---");

        if (channelWidth < minimalCellMinWidth)
        {
            // No room for any text -- a colour swatch still shows the pattern's
            // shape (which channels/rows have notes) across many channels.
            g.setColour(emptyNote ? emptyColor.withAlpha(0.4f) : noteColor.withAlpha(0.85f));
            g.fillRect(x + 1, y + 2, juce::jmax(1, channelWidth - 2), rowHeight - 4);
            return;
        }

        if (channelWidth < compactCellMinWidth)
        {
            // Minimal: note only, small font.
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 8.0f, juce::Font::plain));
            g.setColour(emptyNote ? emptyColor : noteColor);
            g.drawText(noteStr, x + 1, y, channelWidth - 2, rowHeight, juce::Justification::centred);
            return;
        }

        if (channelWidth < fullCellMinWidth)
        {
            // Compact: note + instrument, no volume/effect columns.
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
            const int noteW = channelWidth * 6 / 10;
            g.setColour(emptyNote ? emptyColor : noteColor);
            g.drawText(noteStr, x + 2, y, noteW - 2, rowHeight, juce::Justification::centredLeft);

            auto instStr = note.getInstString();
            g.setColour(instStr == "--" ? emptyColor : instColor);
            g.drawText(instStr, x + noteW, y, juce::jmax(0, channelWidth - noteW - 2), rowHeight,
                       juce::Justification::centredLeft);
            return;
        }

        // Full: note(3 chars) inst(2 chars) vol(3 chars) effect(3 chars), positions
        // scaled from the original 100px-wide layout so wider fit-to-width columns
        // (fewer channels) still look proportioned rather than left-clipped.
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::plain));
        const float scale = channelWidth / (float) defaultChannelWidth;
        auto scaled = [scale](int px) { return juce::roundToInt(px * scale); };

        g.setColour(emptyNote ? emptyColor : noteColor);
        g.drawText(noteStr, x + scaled(3), y, scaled(21), rowHeight, juce::Justification::centredLeft);

        auto instStr = note.getInstString();
        g.setColour(instStr == "--" ? emptyColor : instColor);
        g.drawText(instStr, x + scaled(27), y, scaled(17), rowHeight, juce::Justification::centredLeft);

        auto volStr = note.getVolString();
        g.setColour(volStr == ".." ? emptyColor : volColor);
        g.drawText(volStr, x + scaled(44), y, scaled(24), rowHeight, juce::Justification::centredLeft);

        auto effStr = note.getEffectString();
        g.setColour(effStr == "..." ? emptyColor : effectColor);
        g.drawText(effStr, x + scaled(68), y, scaled(30), rowHeight, juce::Justification::centredLeft);
    }

    void scrollToRow(int row)
    {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        {
            int y         = headerHeight + row * rowHeight;
            int viewTop    = vp->getViewPositionY();
            int viewBottom = viewTop + vp->getViewHeight();

            if (y < viewTop)
                vp->setViewPosition(vp->getViewPositionX(), y);
            else if (y + rowHeight > viewBottom)
                vp->setViewPosition(vp->getViewPositionX(),
                                    y + rowHeight - vp->getViewHeight());
        }
    }

    const ModulePattern* pattern = nullptr;
    int patternIndex = 0;
    int currentRow   = -1;
    int channelWidth = defaultChannelWidth;

    // Dark tracker colour scheme
    const juce::Colour bgColor        { 0xff141420 };
    const juce::Colour headerBgColor  { 0xff0c0c18 };
    const juce::Colour headerTextColor{ 0xff8899aa };
    const juce::Colour rowEvenBg      { 0xff141420 };
    const juce::Colour rowOddBg       { 0xff181828 };
    const juce::Colour beatRowBg      { 0xff1e1e32 };
    const juce::Colour currentRowBg   { 0xff1e3a6e };
    const juce::Colour separatorColor { 0xff252540 };
    const juce::Colour rowNumColor    { 0xff556677 };
    const juce::Colour emptyColor     { 0xff303050 };
    const juce::Colour noteColor      { 0xff77ee77 };
    const juce::Colour instColor      { 0xffffcc44 };
    const juce::Colour volColor       { 0xff44ccff };
    const juce::Colour effectColor    { 0xffff9944 };
};

// ---- ModulePatternViewer ----

ModulePatternViewer::ModulePatternViewer()
{
    titleLabel = std::make_unique<juce::Label>("title", "Pattern Viewer");
    titleLabel->setColour(juce::Label::textColourId, juce::Colour(0xff99bbdd));
    titleLabel->setFont(juce::Font(13.0f, juce::Font::bold));
    addAndMakeVisible(*titleLabel);

    patternLabel = std::make_unique<juce::Label>("pat", "");
    patternLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(*patternLabel);

    prevButton = std::make_unique<juce::TextButton>("<");
    prevButton->onClick = [this] { showPattern(currentPatternIndex - 1); };
    addAndMakeVisible(*prevButton);

    nextButton = std::make_unique<juce::TextButton>(">");
    nextButton->onClick = [this] { showPattern(currentPatternIndex + 1); };
    addAndMakeVisible(*nextButton);

    statusLabel = std::make_unique<juce::Label>("status", "");
    statusLabel->setJustificationType(juce::Justification::centred);
    statusLabel->setColour(juce::Label::textColourId, juce::Colour(0xff6a6a8a));
    statusLabel->setVisible(false);
    addAndMakeVisible(*statusLabel);

    grid     = std::make_unique<PatternGridComponent>();
    viewport = std::make_unique<juce::Viewport>();
    viewport->setViewedComponent(grid.get(), false);
    viewport->setScrollBarsShown(true, true);
    viewport->setVisible(false);
    addAndMakeVisible(*viewport);

    updateNavButtons();
}

ModulePatternViewer::~ModulePatternViewer() = default;

void ModulePatternViewer::setPatternData(const ModulePatternData* data)
{
    patternData         = data;
    currentPatternIndex = 0;

#ifdef SOUNDPLAYER_OPENMPT_SUPPORT
    if (data != nullptr && data->isLoaded() && data->getNumPatterns() > 0)
    {
        statusLabel->setVisible(false);
        viewport->setVisible(true);
        showPattern(0);
        return;
    }
    statusLabel->setText(data != nullptr ? "Failed to read pattern data."
                                         : "No module file loaded.",
                         juce::dontSendNotification);
#else
    statusLabel->setText("Module pattern viewer requires libopenmpt.\n"
                         "Install libopenmpt-dev and rebuild.",
                         juce::dontSendNotification);
    juce::ignoreUnused(data);
#endif

    grid->setPattern(nullptr, 0);
    statusLabel->setVisible(true);
    viewport->setVisible(false);
    updateNavButtons();
}

void ModulePatternViewer::setCurrentRow(int row)
{
    grid->setCurrentRow(row);
}

void ModulePatternViewer::setFitChannelsToWidth(bool shouldFit)
{
    fitChannelsToWidth = shouldFit;
    // No scrollbar needed once every channel is made to fit -- and hiding it
    // stops it from stealing width the fit calculation is counting on.
    viewport->setScrollBarsShown(!shouldFit, !shouldFit);
    refitGrid();
}

void ModulePatternViewer::refitGrid()
{
    if (patternData == nullptr || !patternData->isLoaded()) return;

    const int fitWidth = fitChannelsToWidth ? viewport->getWidth() : -1;
    grid->setPattern(patternData->getPattern(currentPatternIndex), currentPatternIndex, fitWidth);
}

void ModulePatternViewer::setPlaybackPosition(int patternIndex, int row)
{
    if (patternIndex >= 0 && patternIndex != currentPatternIndex)
        showPattern(patternIndex);
    if (row >= 0)
        grid->setCurrentRow(row);
}

void ModulePatternViewer::showPattern(int index)
{
    if (patternData == nullptr || !patternData->isLoaded()) return;

    int numPats = patternData->getNumPatterns();
    if (numPats == 0) return;

    currentPatternIndex = juce::jlimit(0, numPats - 1, index);
    refitGrid();

    auto title = patternData->getTitle();
    titleLabel->setText(title.isNotEmpty() ? title : "Pattern Viewer",
                        juce::dontSendNotification);
    patternLabel->setText("Pattern: " + juce::String(currentPatternIndex + 1)
                          + " / " + juce::String(numPats),
                          juce::dontSendNotification);
    updateNavButtons();
}

void ModulePatternViewer::updateNavButtons()
{
    bool hasData = patternData != nullptr
                   && patternData->isLoaded()
                   && patternData->getNumPatterns() > 0;

    prevButton->setEnabled(hasData && currentPatternIndex > 0);
    nextButton->setEnabled(hasData && currentPatternIndex < patternData->getNumPatterns() - 1);
}

void ModulePatternViewer::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e0e1a));

    g.setColour(juce::Colour(0xff252540));
    g.drawRect(getLocalBounds(), 1);
}

void ModulePatternViewer::resized()
{
    constexpr int barH = 28;
    auto area = getLocalBounds().reduced(1);

    auto bar = area.removeFromTop(barH).reduced(4, 3);
    nextButton->setBounds(bar.removeFromRight(26));
    bar.removeFromRight(2);
    prevButton->setBounds(bar.removeFromRight(26));
    bar.removeFromRight(6);
    patternLabel->setBounds(bar.removeFromRight(110));
    bar.removeFromRight(4);
    titleLabel->setBounds(bar);

    viewport->setBounds(area);
    statusLabel->setBounds(area);

    refitGrid();
}
