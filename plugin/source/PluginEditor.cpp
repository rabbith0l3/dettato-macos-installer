#include "PluginEditor.h"

namespace dettato
{
namespace
{
const auto background = juce::Colour(0xff111316);
const auto panel = juce::Colour(0xff1b1f24);
const auto border = juce::Colour(0xff30363d);
const auto text = juce::Colour(0xfff2f0ea);
const auto muted = juce::Colour(0xff9aa4ae);
const auto accent = juce::Colour(0xffe66a9b);
const auto success = juce::Colour(0xff77d9a4);
const auto failure = juce::Colour(0xffff8c85);
} // namespace

DettatoAudioProcessorEditor::DettatoAudioProcessorEditor(DettatoAudioProcessor& owner)
    : AudioProcessorEditor(owner), processor(owner)
{
    setLookAndFeel(&lookAndFeel);
    lookAndFeel.setColour(juce::TextButton::buttonColourId, panel);
    lookAndFeel.setColour(juce::TextButton::buttonOnColourId, accent);
    lookAndFeel.setColour(juce::TextButton::textColourOffId, text);
    lookAndFeel.setColour(juce::Label::textColourId, text);

    for (auto* button : { &chooseButton, &cancelButton, &previewButton, &saveButton })
        addAndMakeVisible(*button);

    chooseButton.onClick = [this] { chooseAudio(); };
    cancelButton.onClick = [this] { processor.cancelTranscription(); };
    previewButton.onClick = [this]
    {
        processor.togglePreview();
        previewButton.setButtonText(processor.previewPlaying() ? "Stop MIDI preview" : "Play MIDI preview");
    };
    saveButton.onClick = [this] { saveMidi(); };

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    tracksLabel.setJustificationType(juce::Justification::topLeft);
    tracksLabel.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(tracksLabel);
    addAndMakeVisible(progressBar);

    setSize(720, 520);
    startTimerHz(10);
}

DettatoAudioProcessorEditor::~DettatoAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void DettatoAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(accent);
    g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
    g.drawText("DETTATO", 28, 22, 250, 40, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("Audio in. A complete MIDI arrangement out.", 190, 32, 480, 24, juce::Justification::centredRight);

    g.setColour(panel);
    g.fillRoundedRectangle(dropArea.toFloat(), 14.0f);
    g.setColour(dragHover ? accent : border);
    g.drawRoundedRectangle(dropArea.toFloat().reduced(1.0f), 14.0f, dragHover ? 2.5f : 1.5f);

    const auto source = processor.sourceFile();
    g.setColour(text);
    g.setFont(juce::FontOptions(21.0f, juce::Font::bold));
    g.drawText(source.existsAsFile() ? source.getFileName() : "Drop an audio file here",
               dropArea.reduced(24).removeFromTop(86), juce::Justification::centred);
    g.setColour(muted);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("WAV  /  MP3  /  FLAC  /  M4A  /  AIFF  /  OGG  /  OPUS",
               dropArea.reduced(24).withTrimmedTop(104).removeFromTop(28), juce::Justification::centred);

    g.setColour(panel);
    g.fillRoundedRectangle(midiArea.toFloat(), 12.0f);
    const auto ready = processor.jobSnapshot().status == JobStatus::succeeded && processor.midiFile().existsAsFile();
    g.setColour(ready ? success : border);
    g.drawRoundedRectangle(midiArea.toFloat().reduced(1.0f), 12.0f, 1.5f);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText(ready ? "↗  Drag MIDI into your DAW" : "MIDI output",
               midiArea.reduced(18).removeFromTop(34), juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(ready ? processor.midiFile().getFileName() : "Your transcription will appear here",
               midiArea.reduced(18).withTrimmedTop(42), juce::Justification::topLeft);
}

void DettatoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    dropArea = area.removeFromTop(182).withTrimmedTop(82);
    area.removeFromTop(18);
    statusLabel.setBounds(area.removeFromTop(26));
    progressBar.setBounds(area.removeFromTop(24).reduced(0, 7));
    area.removeFromTop(8);
    tracksLabel.setBounds(area.removeFromTop(60));
    area.removeFromTop(8);
    midiArea = area.removeFromTop(88);
    area.removeFromTop(16);
    auto buttons = area.removeFromTop(38);
    chooseButton.setBounds(buttons.removeFromLeft(140));
    buttons.removeFromLeft(10);
    cancelButton.setBounds(buttons.removeFromLeft(90));
    buttons.removeFromLeft(10);
    previewButton.setBounds(buttons.removeFromLeft(160));
    buttons.removeFromLeft(10);
    saveButton.setBounds(buttons);
}

bool DettatoAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && EngineClient::isSupportedAudioFile(juce::File(files[0]));
}

void DettatoAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void DettatoAudioProcessorEditor::fileDragExit(const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void DettatoAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    dragHover = false;
    if (files.size() == 1)
        processor.startTranscription(juce::File(files[0]));
    repaint();
}

void DettatoAudioProcessorEditor::chooseAudio()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Choose an audio file", juce::File(), "*.wav;*.mp3;*.flac;*.ogg;*.m4a;*.aiff;*.aif;*.opus");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser& chooser)
                             {
                                 const auto result = chooser.getResult();
                                 if (result.existsAsFile())
                                     processor.startTranscription(result);
                                 fileChooser.reset();
                             });
}

void DettatoAudioProcessorEditor::saveMidi()
{
    const auto midi = processor.midiFile();
    if (!midi.existsAsFile())
        return;
    const auto suggested = processor.sourceFile().getSiblingFile(processor.sourceFile().getFileNameWithoutExtension() + " - Dettato.mid");
    fileChooser = std::make_unique<juce::FileChooser>("Save MIDI", suggested, "*.mid");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                             [this, midi](const juce::FileChooser& chooser)
                             {
                                 auto destination = chooser.getResult();
                                 if (destination.getFullPathName().isNotEmpty())
                                 {
                                     if (!destination.hasFileExtension(".mid"))
                                         destination = destination.withFileExtension(".mid");
                                     if (destination.existsAsFile())
                                         destination.deleteFile();
                                     if (!midi.copyFileTo(destination))
                                         statusLabel.setText("Could not save the MIDI file.", juce::dontSendNotification);
                                 }
                                 fileChooser.reset();
                             });
}

juce::String DettatoAudioProcessorEditor::tracksText(const juce::String& json) const
{
    if (json.isEmpty())
        return "MIDI restored from the DAW session.";
    const auto summary = juce::JSON::parse(json);
    const auto* tracks = summary.getProperty("tracks", {}).getArray();
    if (tracks == nullptr)
        return {};
    juce::StringArray lines;
    for (const auto& track : *tracks)
    {
        const auto name = track.getProperty("name", "Track").toString();
        const auto notes = static_cast<int>(track.getProperty("notes", 0));
        lines.add(name + "  •  " + juce::String(notes) + (notes == 1 ? " note" : " notes"));
    }
    return lines.joinIntoString("     ");
}

void DettatoAudioProcessorEditor::timerCallback()
{
    const auto snapshot = processor.jobSnapshot();
    const auto ready = snapshot.status == JobStatus::succeeded && processor.midiFile().existsAsFile();
    cancelButton.setEnabled(snapshot.status == JobStatus::running);
    previewButton.setEnabled(ready);
    saveButton.setEnabled(ready);
    progressBar.setVisible(snapshot.status == JobStatus::running);
    previewButton.setButtonText(processor.previewPlaying() ? "Stop MIDI preview" : "Play MIDI preview");

    switch (snapshot.status)
    {
        case JobStatus::idle:
            statusLabel.setText("Ready — drop audio or choose a file", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, muted);
            tracksLabel.setText({}, juce::dontSendNotification);
            break;
        case JobStatus::running:
            statusLabel.setText("Analysing… First run also downloads the models; CPU transcription can take several minutes.",
                                juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, text);
            tracksLabel.setText("The DAW audio thread remains live. You can cancel at any time.", juce::dontSendNotification);
            break;
        case JobStatus::succeeded:
            statusLabel.setText("Transcription complete", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, success);
            tracksLabel.setText(tracksText(snapshot.summary), juce::dontSendNotification);
            break;
        case JobStatus::failed:
            statusLabel.setText(juce::String(snapshot.error), juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, failure);
            tracksLabel.setText("Choose another file or try again.", juce::dontSendNotification);
            break;
    }
    repaint();
}

void DettatoAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    dragArmed = midiArea.contains(event.getPosition()) && processor.midiFile().existsAsFile();
}

void DettatoAudioProcessorEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (!dragArmed || event.getDistanceFromDragStart() < 6)
        return;
    dragArmed = false;
    const auto midi = processor.midiFile();
    if (midi.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles({ midi.getFullPathName() }, false, this);
}
} // namespace dettato
