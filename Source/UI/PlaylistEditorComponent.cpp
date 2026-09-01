#include "PlaylistEditorComponent.h"

PlaylistEditorComponent::PlaylistEditorComponent(PlaylistManager* playlist)
    : playlistManager(playlist)
{
    // Create table
    playlistTable = std::make_unique<juce::TableListBox>("Playlist", this);
    playlistTable->getHeader().addColumn("Track", 1, 200);
    playlistTable->getHeader().addColumn("Artist", 2, 150);
    playlistTable->getHeader().addColumn("Album", 3, 150);
    playlistTable->getHeader().addColumn("Layout", 4, 80);
    playlistTable->getHeader().addColumn("Duration", 5, 80);
    playlistTable->setMultipleSelectionEnabled(false);
    addAndMakeVisible(*playlistTable);

    // Add button
    addButton = std::make_unique<juce::TextButton>("Add Files");
    addButton->onClick = [this] { addFilesFromDialog(); };
    addAndMakeVisible(*addButton);

    // Add Stream button
    addStreamButton = std::make_unique<juce::TextButton>("Add Stream");
    addStreamButton->onClick = [this] { addStreamFromDialog(); };
    addAndMakeVisible(*addStreamButton);

    // Remove button
    removeButton = std::make_unique<juce::TextButton>("Remove");
    removeButton->onClick = [this]
    {
        int selected = playlistTable->getSelectedRow();
        if (selected >= 0)
            playlistManager->removeItem(selected);
    };
    addAndMakeVisible(*removeButton);

    // Clear button
    clearButton = std::make_unique<juce::TextButton>("Clear All");
    clearButton->onClick = [this] { playlistManager->clearPlaylist(); };
    addAndMakeVisible(*clearButton);

    // Save button
    saveButton = std::make_unique<juce::TextButton>("Save...");
    saveButton->onClick = [this] { savePlaylistDialog(); };
    addAndMakeVisible(*saveButton);

    // Load button
    loadButton = std::make_unique<juce::TextButton>("Load...");
    loadButton->onClick = [this] { loadPlaylistDialog(); };
    addAndMakeVisible(*loadButton);

    playlistManager->addChangeListener(this);
}

PlaylistEditorComponent::~PlaylistEditorComponent()
{
    playlistManager->removeChangeListener(this);
}

void PlaylistEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey.withAlpha(0.5f));
}

void PlaylistEditorComponent::resized()
{
    auto area = getLocalBounds().reduced(5);

    auto buttonArea = area.removeFromBottom(40);
    buttonArea.removeFromLeft(5);
    addButton->setBounds(buttonArea.removeFromLeft(90));
    buttonArea.removeFromLeft(4);
    addStreamButton->setBounds(buttonArea.removeFromLeft(90));
    buttonArea.removeFromLeft(4);
    removeButton->setBounds(buttonArea.removeFromLeft(80));
    buttonArea.removeFromLeft(4);
    clearButton->setBounds(buttonArea.removeFromLeft(80));
    buttonArea.removeFromLeft(4);
    saveButton->setBounds(buttonArea.removeFromLeft(70));
    buttonArea.removeFromLeft(4);
    loadButton->setBounds(buttonArea.removeFromLeft(70));

    area.removeFromBottom(5);
    playlistTable->setBounds(area);
}

void PlaylistEditorComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    playlistTable->updateContent();
}

int PlaylistEditorComponent::getNumRows()
{
    return playlistManager->getItems().size();
}

void PlaylistEditorComponent::paintRowBackground(juce::Graphics& g, int rowNumber, 
                                                 int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colours::lightblue);
    else if (rowNumber % 2 == 0)
        g.fillAll(juce::Colours::darkgrey);
}

void PlaylistEditorComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                        int width, int height, bool rowIsSelected)
{
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);

    const auto& items = playlistManager->getItems();
    if (juce::isPositiveAndBelow(rowNumber, items.size()))
    {
        const auto& item = items[rowNumber];
        juce::String text;

        switch (columnId)
        {
            case 1: text = item.name; break;
            case 2: text = item.artist; break;
            case 3: text = item.album; break;
            case 4:
                text = item.channelLayout;
                break;
            case 5:
            {
                if (item.isStream)
                    text = "STREAM";
                else
                {
                    int seconds = (int)item.duration;
                    text = juce::String::formatted("%d:%02d", seconds / 60, seconds % 60);
                }
                break;
            }
        }

        g.drawText(text, 5, 0, width - 10, height, juce::Justification::centredLeft);
    }
}

void PlaylistEditorComponent::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&)
{
    playlistManager->selectItem(rowNumber);
}

void PlaylistEditorComponent::deleteKeyPressed(int lastRowSelected)
{
    if (lastRowSelected >= 0)
        playlistManager->removeItem(lastRowSelected);
}

void PlaylistEditorComponent::addStreamFromDialog()
{
    auto* dialog = new juce::AlertWindow("Add Stream", "Enter stream URL:", juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("url", "http://", "URL:");
    dialog->addTextEditor("name", "", "Display name (optional):");
    dialog->addButton("Add", 1);
    dialog->addButton("Cancel", 0);

    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create([this, dialog](int result)
        {
            if (result == 1)
            {
                auto url  = dialog->getTextEditorContents("url").trim();
                auto name = dialog->getTextEditorContents("name").trim();
                if (url.isNotEmpty() && url != "http://")
                    playlistManager->addStream(url, name);
            }
        }),
        true); // deleteWhenDismissed
}

void PlaylistEditorComponent::addFilesFromDialog()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select audio files...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.mp3;*.wav;*.flac;*.ogg;*.xtp;*.xtd;*.mod;*.xm;*.it;*.s3m;*.mptm;*.umx;*.ahx;*.ams;*.amf;*.med;*.mo3;*.mtm;*.okt;*.oct;*.stm;*.669;*.umf;*.drm;*.digi;*.far;*.mdl;*.mt2;*.dcm;*.dlm;*.ptm;*.pcm;*.dbm;*.dsm;*.cdm;*.hvl;*.c67;*.sfx;*.imf;*.u2b;*.psm;*.stp;*.symmod",
        // zenity 4.x (GTK4/libadwaita rewrite) has a known --file-filter regression
        // that silently drops some extensions from a multi-pattern filter, so use
        // JUCE's own cross-platform browser instead of shelling out to it.
        false);

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
        [this](const juce::FileChooser& fc)
        {
            auto results = fc.getResults();
            if (results.size() > 0)
                playlistManager->addFiles(results);
        });
}

void PlaylistEditorComponent::savePlaylistDialog()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save playlist...",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.xml");

    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest != juce::File{})
            {
                if (dest.getFileExtension().isEmpty())
                    dest = dest.withFileExtension("xml");
                playlistManager->savePlaylist(dest);
            }
        });
}

void PlaylistEditorComponent::loadPlaylistDialog()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load playlist...",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.xml");

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode,
        [this](const juce::FileChooser& fc)
        {
            auto src = fc.getResult();
            if (src != juce::File{})
                playlistManager->loadPlaylist(src);
        });
}
