#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

#include <cassert>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI initialiser;
    dettato::DettatoAudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);

    juce::AudioBuffer<float> audio(2, 128);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 128; ++sample)
            audio.setSample(channel, sample, channel == 0 ? 0.25f : -0.5f);
    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);

    for (int sample = 0; sample < 128; ++sample)
    {
        assert(audio.getSample(0, sample) == 0.25f);
        assert(audio.getSample(1, sample) == -0.5f);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    assert(editor != nullptr);
    assert(editor->getWidth() >= 600 && editor->getHeight() >= 400);
    editor.reset();

    if (argc == 2)
    {
        ::setenv("DETTATO_ENGINE_PATH", argv[1], 1);
        const auto pidLog = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("dettato-host-smoke-pids.txt");
        pidLog.deleteFile();
        ::setenv("FAKE_ENGINE_PID_LOG", pidLog.getFullPathName().toRawUTF8(), 1);
        const auto input = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("dettato-host-smoke.wav");
        input.replaceWithData("fake audio", 10);
        processor.startTranscription(input);
        for (int attempt = 0; attempt < 500
             && processor.jobSnapshot().status == dettato::JobStatus::running; ++attempt)
            juce::Thread::sleep(10);
        assert(processor.jobSnapshot().status == dettato::JobStatus::succeeded);
        assert(processor.midiFile().existsAsFile());
        assert(processor.togglePreview());
        processor.processBlock(audio, midi);
        assert(processor.previewPlaying());
        processor.togglePreview();

        juce::MemoryBlock savedState;
        processor.getStateInformation(savedState);
        dettato::DettatoAudioProcessor restored;
        restored.setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
        assert(restored.jobSnapshot().status == dettato::JobStatus::succeeded);
        assert(restored.midiFile() == processor.midiFile());

        processor.startTranscription(input);
        for (int attempt = 0; attempt < 500
             && processor.jobSnapshot().status == dettato::JobStatus::running; ++attempt)
            juce::Thread::sleep(10);
        assert(processor.jobSnapshot().status == dettato::JobStatus::succeeded);
        juce::StringArray processIds;
        processIds.addLines(pidLog.loadFileAsString());
        processIds.removeEmptyStrings();
        assert(processIds.size() == 1);
    }

    processor.releaseResources();

    std::cout << "VST/AU/AAX shared processor: audio passthrough and editor lifecycle passed\n";
}
