#include "PluginChainComponent.h"

// ---------------------------------------------------------------------------
// Per-row component: plugin name + bypass toggle
// ---------------------------------------------------------------------------
class PluginRowComponent : public juce::Component
{
public:
    std::function<void(bool)> onBypassChanged;

    PluginRowComponent()
    {
        bypassButton.setButtonText("Bypass");
        bypassButton.setClickingTogglesState(true);
        bypassButton.setToggleState(false, juce::dontSendNotification);
        bypassButton.onClick = [this] {
            if (onBypassChanged)
                onBypassChanged(bypassButton.getToggleState());
        };
        addAndMakeVisible(bypassButton);
        addAndMakeVisible(nameLabel);
        nameLabel.setJustificationType(juce::Justification::centredLeft);
    }

    void setPluginName(const juce::String& name)   { nameLabel.setText(name, juce::dontSendNotification); }
    void setBypassed(bool bypassed)
    {
        bypassButton.setToggleState(bypassed, juce::dontSendNotification);
        nameLabel.setEnabled(!bypassed);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);
        bypassButton.setBounds(bounds.removeFromRight(70));
        bounds.removeFromRight(4);
        nameLabel.setBounds(bounds);
    }

private:
    juce::Label      nameLabel;
    juce::TextButton bypassButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRowComponent)
};

// ---------------------------------------------------------------------------
// PluginChainComponent
// ---------------------------------------------------------------------------
PluginChainComponent::PluginChainComponent(PluginHostManager* hostMgr, PluginChain* pluginChain)
    : hostManager(hostMgr), chain(pluginChain)
{
    pluginList.setModel(this);
    pluginList.setRowHeight(24);
    addAndMakeVisible(pluginList);

    addBtn     = std::make_unique<juce::TextButton>("Add");
    removeBtn  = std::make_unique<juce::TextButton>("Remove");
    moveUpBtn  = std::make_unique<juce::TextButton>("Up");
    moveDownBtn= std::make_unique<juce::TextButton>("Down");
    scanBtn    = std::make_unique<juce::TextButton>("Scan");

    addBtn->onClick      = [this] { onAddClicked(); };
    removeBtn->onClick   = [this] { onRemoveClicked(); };
    moveUpBtn->onClick   = [this] { onMoveUpClicked(); };
    moveDownBtn->onClick = [this] { onMoveDownClicked(); };
    scanBtn->onClick     = [this] { onScanClicked(); };

    addAndMakeVisible(*addBtn);
    addAndMakeVisible(*removeBtn);
    addAndMakeVisible(*moveUpBtn);
    addAndMakeVisible(*moveDownBtn);
    addAndMakeVisible(*scanBtn);

    updateButtonStates();
}

PluginChainComponent::~PluginChainComponent()
{
    pluginList.setModel(nullptr);
    openEditors.clear();
}

void PluginChainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(12.0f);
    g.drawText("FX Chain", getLocalBounds().removeFromTop(18).reduced(4, 0),
               juce::Justification::centredLeft);
}

void PluginChainComponent::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(18); // title

    // Button row at bottom
    auto btnRow = area.removeFromBottom(24);
    const int btnW = 60;
    addBtn->setBounds(btnRow.removeFromLeft(btnW));
    btnRow.removeFromLeft(2);
    removeBtn->setBounds(btnRow.removeFromLeft(btnW));
    btnRow.removeFromLeft(2);
    moveUpBtn->setBounds(btnRow.removeFromLeft(30));
    btnRow.removeFromLeft(2);
    moveDownBtn->setBounds(btnRow.removeFromLeft(30));
    btnRow.removeFromLeft(8);
    scanBtn->setBounds(btnRow.removeFromLeft(btnW));

    area.removeFromBottom(2);
    pluginList.setBounds(area);
}

// ---------------------------------------------------------------------------
// ListBoxModel
// ---------------------------------------------------------------------------
int PluginChainComponent::getNumRows()
{
    return chain->size();
}

void PluginChainComponent::paintListBoxItem(int, juce::Graphics&, int, int, bool)
{
    // Painting is handled by the row component (refreshComponentForRow)
}

juce::Component* PluginChainComponent::refreshComponentForRow(
    int rowNumber, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    auto* row = dynamic_cast<PluginRowComponent*>(existingComponentToUpdate);
    if (row == nullptr)
    {
        row = new PluginRowComponent();
        row->onBypassChanged = [this, rowNumber](bool bypassed)
        {
            chain->setBypass(rowNumber, bypassed);
        };
    }

    if (rowNumber < chain->size())
    {
        row->setPluginName(chain->getPluginName(rowNumber));
        row->setBypassed(chain->isBypassed(rowNumber));
        // Re-bind index (lambdas capture by value at creation; update via setter)
        row->onBypassChanged = [this, rowNumber](bool bypassed)
        {
            chain->setBypass(rowNumber, bypassed);
        };
    }
    return row;
}

void PluginChainComponent::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    openEditorForRow(row);
}

void PluginChainComponent::selectedRowsChanged(int)
{
    updateButtonStates();
}

void PluginChainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    pluginList.updateContent();
    updateButtonStates();
}

// ---------------------------------------------------------------------------
// Button handlers
// ---------------------------------------------------------------------------
void PluginChainComponent::onAddClicked()
{
    const auto& types = hostManager->getKnownPlugins().getTypes();

    if (types.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "No plugins found",
            "Click Scan to search for installed plugins first.");
        return;
    }

    juce::PopupMenu menu;

    // Group by format
    juce::StringArray formats;
    for (const auto& t : types)
        formats.addIfNotAlreadyThere(t.pluginFormatName);
    formats.sort(false);

    for (const auto& fmt : formats)
    {
        juce::PopupMenu sub;
        int itemId = 1;
        for (const auto& t : types)
        {
            if (t.pluginFormatName == fmt)
            {
                sub.addItem(itemId, t.name + " (" + t.manufacturerName + ")");
                ++itemId;
            }
        }
        menu.addSubMenu(fmt, sub);
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(addBtn.get()),
        [this, types](int result)
        {
            if (result <= 0)
                return;

            // Map result back to PluginDescription
            int idx = result - 1;
            if (idx < types.size())
                loadPluginFromDescription(types[idx]);
        });
}

void PluginChainComponent::onRemoveClicked()
{
    int row = pluginList.getSelectedRow();
    if (row >= 0)
    {
        chain->removePlugin(row);
        pluginList.updateContent();
        updateButtonStates();
    }
}

void PluginChainComponent::onMoveUpClicked()
{
    int row = pluginList.getSelectedRow();
    if (row > 0)
    {
        chain->movePlugin(row, row - 1);
        pluginList.selectRow(row - 1);
        pluginList.updateContent();
    }
}

void PluginChainComponent::onMoveDownClicked()
{
    int row = pluginList.getSelectedRow();
    if (row >= 0 && row < chain->size() - 1)
    {
        chain->movePlugin(row, row + 1);
        pluginList.selectRow(row + 1);
        pluginList.updateContent();
    }
}

void PluginChainComponent::onScanClicked()
{
    scanBtn->setEnabled(false);
    scanBtn->setButtonText("Scanning...");

    juce::Thread::launch([this]
    {
        hostManager->scanDefaultDirectories([](int found, int total)
        {
            juce::ignoreUnused(found, total);
        });

        juce::MessageManager::callAsync([this]
        {
            scanBtn->setEnabled(true);
            scanBtn->setButtonText("Scan");
            pluginList.updateContent();

            int count = hostManager->getKnownPlugins().getNumTypes();
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Scan complete",
                "Found " + juce::String(count) + " plugin(s).");
        });
    });
}

void PluginChainComponent::loadPluginFromDescription(const juce::PluginDescription& desc)
{
    juce::String error;
    // Use current audio device settings — 44100/512 as fallback
    auto instance = hostManager->loadPlugin(desc, 44100.0, 512, error);

    if (instance == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Failed to load plugin",
            error);
        return;
    }

    chain->addPlugin(std::move(instance));
    pluginList.updateContent();
    updateButtonStates();
}

void PluginChainComponent::openEditorForRow(int row)
{
    auto* plugin = chain->getPlugin(row);
    if (plugin == nullptr || !plugin->hasEditor())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "No editor",
            "This plugin has no graphical editor.");
        return;
    }

    auto* editor = plugin->createEditorIfNeeded();
    if (editor == nullptr)
        return;

    auto* window = new juce::DocumentWindow(
        plugin->getName(),
        juce::Colours::darkgrey,
        juce::DocumentWindow::closeButton);

    window->setContentOwned(editor, true);
    window->setResizable(true, false);
    window->setUsingNativeTitleBar(true);
    window->centreWithSize(editor->getWidth(), editor->getHeight());
    window->setVisible(true);

    openEditors.add(window);
}

void PluginChainComponent::updateButtonStates()
{
    const int row   = pluginList.getSelectedRow();
    const int count = chain->size();

    removeBtn->setEnabled(row >= 0);
    moveUpBtn->setEnabled(row > 0);
    moveDownBtn->setEnabled(row >= 0 && row < count - 1);
}
