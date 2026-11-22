#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cassert>
#include <iostream>

int main(int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI initialiser;
    assert(argc == 2);

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, argv[1]);
    assert(!descriptions.isEmpty());

    juce::String error;
    juce::AudioPluginFormatManager manager;
    manager.addFormat(new juce::VST3PluginFormat());
    auto plugin = manager.createPluginInstance(*descriptions[0], 48000.0, 128, error);
    if (plugin == nullptr)
        std::cerr << error << "\n";
    assert(plugin != nullptr);
    assert(plugin->getName() == "Dettato");

    plugin->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> audio(2, 128);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 128; ++sample)
            audio.setSample(channel, sample, channel == 0 ? 0.125f : -0.25f);
    juce::MidiBuffer midi;
    plugin->processBlock(audio, midi);
    for (int sample = 0; sample < 128; ++sample)
    {
        assert(audio.getSample(0, sample) == 0.125f);
        assert(audio.getSample(1, sample) == -0.25f);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditor());
    assert(editor != nullptr);
    editor.reset();
    plugin->releaseResources();
    plugin.reset();

    std::cout << "External VST3 bundle loaded, processed audio and created its editor\n";
}
