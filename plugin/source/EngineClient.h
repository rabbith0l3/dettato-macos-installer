#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <sys/types.h>

namespace dettato
{
class EngineClient final : private juce::Thread
{
public:
    using EventCallback = std::function<void(const juce::var&)>;

    EngineClient();
    ~EngineClient() override;

    void setEventCallback(EventCallback callback);
    bool transcribe(const juce::String& id,
                    const juce::File& input,
                    const juce::File& output,
                    juce::String& error);
    void cancel();
    void shutdown();

    static juce::File findEngine();
    static bool isSupportedAudioFile(const juce::File& file);

private:
    void run() override;
    bool ensureStarted(juce::String& error);
    juce::String activeJobId() const;
    void emit(const juce::var& event);
    bool writeRequest(const juce::String& line);
    bool sendJob(const juce::String& id, const juce::File& input, const juce::File& output);

    mutable juce::CriticalSection lock;
    pid_t processId { -1 };
    int inputDescriptor { -1 };
    int outputDescriptor { -1 };
    juce::String activeId;
    bool jobRunning { false };
    EventCallback callback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EngineClient)
};
} // namespace dettato
