#include "EngineClient.h"

#include <juce_core/juce_core.h>

#include <cassert>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <mutex>
#include <vector>

int main(int argc, char** argv)
{
    assert(argc == 2);
    ::setenv("DETTATO_ENGINE_PATH", argv[1], 1);

    dettato::EngineClient client;
    std::mutex mutex;
    std::vector<juce::var> events;
    client.setEventCallback([&](const juce::var& event)
    {
        const std::lock_guard lock(mutex);
        events.push_back(event);
    });

    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("dettato-engine-client-test");
    folder.createDirectory();
    const auto input = folder.getChildFile("input.wav");
    input.replaceWithData("fake", 4);

    auto run = [&](const juce::String& id)
    {
        juce::String error;
        const auto output = folder.getChildFile(id + ".mid");
        const bool started = client.transcribe(id, input, output, error);
        if (!started)
            std::cerr << error << "\n";
        assert(started);
        for (int attempt = 0; attempt < 500; ++attempt)
        {
            {
                const std::lock_guard lock(mutex);
                for (const auto& event : events)
                    if (event.getProperty("id", {}).toString() == id)
                        return event;
            }
            juce::Thread::sleep(10);
        }
        assert(false && "timed out waiting for engine result");
        return juce::var();
    };

    const auto first = run("one");
    const auto second = run("two");
    assert(first.getProperty("status", {}).toString() == "succeeded");
    assert(second.getProperty("status", {}).toString() == "succeeded");
    assert(static_cast<int>(first.getProperty("pid", 0)) > 0);
    assert(first.getProperty("pid", 0) == second.getProperty("pid", 0));

    ::setenv("FAKE_ENGINE_DELAY_SECONDS", "5", 1);
    juce::String cancelError;
    assert(client.transcribe("cancelled", input, folder.getChildFile("cancelled.mid"), cancelError));
    juce::Thread::sleep(100);
    const auto cancelStarted = std::chrono::steady_clock::now();
    client.cancel();
    const auto cancelMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - cancelStarted).count();
    assert(cancelMilliseconds < 2000);

    ::setenv("FAKE_ENGINE_DELAY_SECONDS", "0", 1);
    const auto afterCancel = run("after-cancel");
    assert(afterCancel.getProperty("status", {}).toString() == "succeeded");
    assert(afterCancel.getProperty("pid", 0) != second.getProperty("pid", 0));

    client.cancel();
    ::setenv("FAKE_ENGINE_MALFORMED", "1", 1);
    const auto malformed = run("malformed");
    assert(malformed.getProperty("status", {}).toString() == "failed");
    assert(malformed.getProperty("error", {}).toString().contains("invalid response"));
    ::unsetenv("FAKE_ENGINE_MALFORMED");

    client.shutdown();
    std::cout << "engine transport completed two jobs in one persistent process\n";
}
