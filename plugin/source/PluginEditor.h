#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace dettato
{
class DettatoAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          private juce::FileDragAndDropTarget
{
public:
    explicit DettatoAudioProcessorEditor(DettatoAudioProcessor&);
    ~DettatoAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int, int) override;
    void timerCallback() override;
    void chooseAudio();
    void saveMidi();
    juce::String tracksText(const juce::String& json) const;

    DettatoAudioProcessor& processor;
    juce::LookAndFeel_V4 lookAndFeel;
    juce::TextButton chooseButton { "Choose audio" };
    juce::TextButton cancelButton { "Cancel" };
    juce::TextButton previewButton { "Play MIDI preview" };
    juce::TextButton saveButton { "Save MIDI as..." };
    juce::Label statusLabel;
    juce::Label tracksLabel;
    double progress { -1.0 };
    juce::ProgressBar progressBar { progress };
    juce::Rectangle<int> dropArea;
    juce::Rectangle<int> midiArea;
    bool dragHover { false };
    bool dragArmed { false };
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DettatoAudioProcessorEditor)
};
} // namespace dettato
