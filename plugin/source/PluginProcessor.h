#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "EngineClient.h"
#include "JobState.h"

#include <atomic>
#include <memory>

namespace dettato
{
class DettatoAudioProcessor final : public juce::AudioProcessor
{
public:
    DettatoAudioProcessor();
    ~DettatoAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void startTranscription(const juce::File& input);
    void cancelTranscription();
    JobSnapshot jobSnapshot() const;
    juce::File sourceFile() const;
    juce::File midiFile() const;
    bool togglePreview();
    bool previewPlaying() const { return previewActive.load(); }

private:
    void handleEngineEvent(const juce::var& event);
    bool validateMidiFile(const juce::File& file) const;
    void stopPreview();

    JobState jobs;
    EngineClient engine;
    juce::CriticalSection fileLock;
    juce::File source;
    juce::File midi;

    juce::SpinLock previewLock;
    juce::Synthesiser previewSynth;
    std::shared_ptr<const juce::MidiBuffer> previewNotes;
    std::atomic<bool> previewActive { false };
    juce::int64 previewPosition { 0 };
    juce::int64 previewLength { 0 };
    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DettatoAudioProcessor)
};
} // namespace dettato
