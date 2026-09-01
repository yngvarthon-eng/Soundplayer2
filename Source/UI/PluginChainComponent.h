#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Plugins/PluginChain.h"
#include "../Plugins/PluginHostManager.h"

/**
 * UI panel showing the plugin effect chain.
 *
 * Provides:
 *   - A list of loaded plugins with per-row bypass toggles
 *   - Add (from scanned list or file browser), Remove, Move Up/Down buttons
 *   - Scan button to find plugins in default directories
 *   - Double-click to open the plugin's native editor window
 */
class PluginChainComponent : public juce::Component,
                             private juce::ListBoxModel,
                             private juce::ChangeListener
{
public:
    PluginChainComponent(PluginHostManager* hostManager, PluginChain* chain);
    ~PluginChainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g,
                          int width, int height,
                          bool rowIsSelected) override;
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected,
                                            juce::Component* existingComponentToUpdate) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void selectedRowsChanged(int lastRowSelected) override;

    // ChangeListener — refreshes list when chain changes externally
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    void onAddClicked();
    void onRemoveClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onScanClicked();
    void loadPluginFromDescription(const juce::PluginDescription& desc);
    void openEditorForRow(int row);
    void updateButtonStates();

    PluginHostManager* hostManager;
    PluginChain*       chain;

    juce::ListBox              pluginList;
    std::unique_ptr<juce::TextButton> addBtn, removeBtn, moveUpBtn, moveDownBtn, scanBtn;

    // Open editor windows — kept alive until user closes them
    juce::OwnedArray<juce::DocumentWindow> openEditors;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChainComponent)
};
