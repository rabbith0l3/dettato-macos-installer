#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace dettato
{
namespace
{
struct PreviewSound final : juce::SynthesiserSound
{
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

struct PreviewVoice final : juce::SynthesiserVoice
{
    using juce::SynthesiserVoice::renderNextBlock;

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<PreviewSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        level = velocity * 0.16f;
        const auto frequency = 440.0 * std::pow(2.0, (midiNoteNumber - 69) / 12.0);
        increment = frequency / getSampleRate();
        phase = 0.0;
        envelope = 0.0;
        releasing = false;
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff)
            releasing = true;
        else
            clearCurrentNote();
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (!isVoiceActive())
            return;

        for (int sample = startSample; sample < startSample + numSamples; ++sample)
        {
            const auto target = releasing ? 0.0 : 1.0;
            envelope += 0.002 * (target - envelope);
            const auto value = static_cast<float>(std::sin(phase * juce::MathConstants<double>::twoPi) * level * envelope);
            for (int channel = 0; channel < output.getNumChannels(); ++channel)
                output.addSample(channel, sample, value);
            phase += increment;
            phase -= std::floor(phase);
        }

        if (releasing && envelope < 0.0001)
            clearCurrentNote();
    }

    double phase { 0.0 };
    double increment { 0.0 };
    double envelope { 0.0 };
    float level { 0.0f };
    bool releasing { false };
};

juce::String summaryText(const juce::var& summary)
{
    return juce::JSON::toString(summary, true);
}
} // namespace

DettatoAudioProcessor::DettatoAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    previewSynth.addSound(new PreviewSound());
    for (int voice = 0; voice < 16; ++voice)
        previewSynth.addVoice(new PreviewVoice());
    engine.setEventCallback([this](const juce::var& event) { handleEngineEvent(event); });
}

DettatoAudioProcessor::~DettatoAudioProcessor()
{
    engine.shutdown();
    stopPreview();
}

void DettatoAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate > 0 ? sampleRate : 44100.0;
    previewSynth.setCurrentPlaybackSampleRate(currentSampleRate);
    stopPreview();
}

void DettatoAudioProcessor::releaseResources()
{
    stopPreview();
}

bool DettatoAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    return !output.isDisabled() && layouts.getMainInputChannelSet() == output;
}

void DettatoAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    const juce::SpinLock::ScopedTryLockType tryLock(previewLock);
    if (!tryLock.isLocked() || !previewActive.load() || previewNotes == nullptr)
        return;

    previewSynth.renderNextBlock(buffer, *previewNotes, static_cast<int>(previewPosition), buffer.getNumSamples());
    previewPosition += buffer.getNumSamples();
    if (previewPosition > previewLength)
        previewActive = false;
}

void DettatoAudioProcessor::startTranscription(const juce::File& input)
{
    if (jobs.snapshot().status == JobStatus::running)
        cancelTranscription();
    else
    {
        jobs.cancel();
        stopPreview();
        const juce::ScopedLock sl(fileLock);
        midi = juce::File();
    }

    if (!EngineClient::isSupportedAudioFile(input))
    {
        jobs.start("invalid");
        jobs.fail("invalid", "Choose a valid WAV, MP3, FLAC, OGG, M4A, AIFF or OPUS audio file.");
        return;
    }

    const auto id = juce::Uuid().toString();
    auto directory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("KobiMusic/Dettato/Results");
    if (!directory.createDirectory())
    {
        jobs.start(id.toStdString());
        jobs.fail(id.toStdString(), "Could not create the Dettato results folder.");
        return;
    }
    const auto output = directory.getChildFile(id + ".mid");
    {
        const juce::ScopedLock sl(fileLock);
        source = input;
        midi = juce::File();
    }
    jobs.start(id.toStdString());
    juce::String error;
    if (!engine.transcribe(id, input, output, error))
        jobs.fail(id.toStdString(), error.toStdString());
}

void DettatoAudioProcessor::cancelTranscription()
{
    engine.cancel();
    jobs.cancel();
    stopPreview();
    const juce::ScopedLock sl(fileLock);
    midi = juce::File();
}

JobSnapshot DettatoAudioProcessor::jobSnapshot() const
{
    return jobs.snapshot();
}

juce::File DettatoAudioProcessor::sourceFile() const
{
    const juce::ScopedLock sl(fileLock);
    return source;
}

juce::File DettatoAudioProcessor::midiFile() const
{
    const juce::ScopedLock sl(fileLock);
    return midi;
}

void DettatoAudioProcessor::handleEngineEvent(const juce::var& event)
{
    const auto id = event.getProperty("id", {}).toString();
    if (event.getProperty("status", {}).toString() == "succeeded")
    {
        const juce::File result(event.getProperty("midi", {}).toString());
        if (!validateMidiFile(result))
        {
            jobs.fail(id.toStdString(), "Dettato Engine finished, but its MIDI file could not be read.");
            return;
        }
        {
            const juce::ScopedLock sl(fileLock);
            midi = result;
        }
        jobs.succeed(id.toStdString(), result.getFullPathName().toStdString(), summaryText(event.getProperty("summary", {})).toStdString());
        return;
    }

    const auto message = event.getProperty("error", "Unknown Dettato Engine error.").toString();
    jobs.fail(id.toStdString(), message.toStdString());
}

bool DettatoAudioProcessor::validateMidiFile(const juce::File& file) const
{
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return false;
    juce::MidiFile midiFile;
    return midiFile.readFrom(stream) && midiFile.getNumTracks() > 0;
}

bool DettatoAudioProcessor::togglePreview()
{
    if (previewActive.load())
    {
        stopPreview();
        return false;
    }

    juce::MidiFile file;
    {
        juce::FileInputStream stream(midiFile());
        if (!stream.openedOk() || !file.readFrom(stream))
            return false;
    }
    file.convertTimestampTicksToSeconds();
    juce::MidiMessageSequence sequence;
    for (int track = 0; track < file.getNumTracks(); ++track)
    {
        sequence.addSequence(*file.getTrack(track), 0.0);
    }
    sequence.updateMatchedPairs();

    auto notes = std::make_shared<juce::MidiBuffer>();
    for (int index = 0; index < sequence.getNumEvents(); ++index)
    {
        const auto& event = sequence.getEventPointer(index)->message;
        if (event.isNoteOnOrOff())
            notes->addEvent(event, juce::roundToInt(event.getTimeStamp() * currentSampleRate));
    }

    const juce::SpinLock::ScopedLockType sl(previewLock);
    previewNotes = std::move(notes);
    previewPosition = 0;
    previewLength = static_cast<juce::int64>(std::llround((sequence.getEndTime() + 1.0) * currentSampleRate));
    previewSynth.allNotesOff(0, false);
    previewActive = true;
    return true;
}

void DettatoAudioProcessor::stopPreview()
{
    const juce::SpinLock::ScopedLockType sl(previewLock);
    previewActive = false;
    previewNotes.reset();
    previewPosition = 0;
}

juce::AudioProcessorEditor* DettatoAudioProcessor::createEditor()
{
    return new DettatoAudioProcessorEditor(*this);
}

void DettatoAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("Dettato");
    state.setProperty("source", sourceFile().getFullPathName(), nullptr);
    state.setProperty("midi", midiFile().getFullPathName(), nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void DettatoAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName("Dettato"))
        return;
    const auto state = juce::ValueTree::fromXml(*xml);
    const juce::File restoredSource(state.getProperty("source").toString());
    const juce::File restoredMidi(state.getProperty("midi").toString());
    {
        const juce::ScopedLock sl(fileLock);
        source = restoredSource.existsAsFile() ? restoredSource : juce::File();
        midi = restoredMidi.existsAsFile() && validateMidiFile(restoredMidi) ? restoredMidi : juce::File();
    }
    if (midiFile().existsAsFile())
    {
        jobs.start("restored");
        jobs.succeed("restored", midiFile().getFullPathName().toStdString(), {});
    }
}
} // namespace dettato

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new dettato::DettatoAudioProcessor();
}
