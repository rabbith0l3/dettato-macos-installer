#include "EngineClient.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace dettato
{
namespace
{
void closeDescriptor(int& descriptor)
{
    if (descriptor >= 0)
    {
        ::close(descriptor);
        descriptor = -1;
    }
}

juce::var failureEvent(const juce::String& id, const juce::String& message)
{
    auto* error = new juce::DynamicObject();
    error->setProperty("id", id);
    error->setProperty("status", "failed");
    error->setProperty("error", message);
    return juce::var(error);
}
} // namespace

EngineClient::EngineClient()
    : juce::Thread("Dettato engine reader")
{
}

EngineClient::~EngineClient()
{
    shutdown();
}

void EngineClient::setEventCallback(EventCallback newCallback)
{
    const juce::ScopedLock sl(lock);
    callback = std::move(newCallback);
}

juce::File EngineClient::findEngine()
{
    const auto overridePath = juce::SystemStats::getEnvironmentVariable("DETTATO_ENGINE_PATH", {});
    if (overridePath.isNotEmpty())
        return juce::File(overridePath);

    const juce::String relative = "KobiMusic/Dettato/DettatoEngine/DettatoEngine";
    const auto userEngine = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                .getChildFile(relative);
    if (userEngine.existsAsFile())
        return userEngine;

    return juce::File("/Library/Application Support").getChildFile(relative);
}

bool EngineClient::isSupportedAudioFile(const juce::File& file)
{
    static const juce::StringArray extensions { ".wav", ".mp3", ".flac", ".ogg", ".m4a", ".aiff", ".aif", ".opus" };
    return file.existsAsFile() && extensions.contains(file.getFileExtension().toLowerCase());
}

bool EngineClient::ensureStarted(juce::String& error)
{
    if (processId > 0 && isThreadRunning())
        return true;

    const auto executable = findEngine();
    if (!executable.existsAsFile())
    {
        error = "Dettato Engine is not installed. Run the Dettato installer again.";
        return false;
    }

    int toChild[2] { -1, -1 };
    int fromChild[2] { -1, -1 };
    if (::pipe(toChild) != 0 || ::pipe(fromChild) != 0)
    {
        closeDescriptor(toChild[0]);
        closeDescriptor(toChild[1]);
        closeDescriptor(fromChild[0]);
        closeDescriptor(fromChild[1]);
        error = "Could not create communication pipes for Dettato Engine.";
        return false;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, toChild[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fromChild[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, toChild[1]);
    posix_spawn_file_actions_addclose(&actions, fromChild[0]);
    if (toChild[0] != STDIN_FILENO)
        posix_spawn_file_actions_addclose(&actions, toChild[0]);
    if (fromChild[1] != STDOUT_FILENO)
        posix_spawn_file_actions_addclose(&actions, fromChild[1]);

    const auto path = executable.getFullPathName();
    const auto pathUtf8 = path.toRawUTF8();
    char* arguments[] { const_cast<char*>(pathUtf8), nullptr };
    const auto result = ::posix_spawn(&processId, pathUtf8, &actions, nullptr, arguments, environ);
    posix_spawn_file_actions_destroy(&actions);
    closeDescriptor(toChild[0]);
    closeDescriptor(fromChild[1]);

    if (result != 0)
    {
        closeDescriptor(toChild[1]);
        closeDescriptor(fromChild[0]);
        processId = -1;
        error = "Could not start Dettato Engine: " + juce::String(std::strerror(result));
        return false;
    }

    inputDescriptor = toChild[1];
    outputDescriptor = fromChild[0];
    startThread(juce::Thread::Priority::normal);
    return true;
}

bool EngineClient::writeRequest(const juce::String& line)
{
    const auto* data = line.toRawUTF8();
    auto remaining = static_cast<size_t>(line.getNumBytesAsUTF8());
    while (remaining > 0)
    {
        const auto written = ::write(inputDescriptor, data, remaining);
        if (written < 0)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        data += written;
        remaining -= static_cast<size_t>(written);
    }
    return true;
}

bool EngineClient::sendJob(const juce::String& id, const juce::File& input, const juce::File& output)
{
    activeId = id;
    jobRunning = true;
    auto* request = new juce::DynamicObject();
    request->setProperty("id", id);
    request->setProperty("input", input.getFullPathName());
    request->setProperty("output", output.getFullPathName());
    const auto sent = writeRequest(juce::JSON::toString(juce::var(request), true) + "\n");
    if (!sent)
        jobRunning = false;
    return sent;
}

bool EngineClient::transcribe(const juce::String& id,
                              const juce::File& input,
                              const juce::File& output,
                              juce::String& error)
{
    {
        const juce::ScopedLock sl(lock);
        if (processId > 0 && isThreadRunning() && !jobRunning && sendJob(id, input, output))
            return true;
    }

    shutdown();

    const juce::ScopedLock sl(lock);
    if (!ensureStarted(error))
        return false;
    if (sendJob(id, input, output))
        return true;

    error = "Could not send the audio file to Dettato Engine.";
    return false;
}

juce::String EngineClient::activeJobId() const
{
    const juce::ScopedLock sl(lock);
    return activeId;
}

void EngineClient::emit(const juce::var& event)
{
    EventCallback target;
    {
        const juce::ScopedLock sl(lock);
        if (event.isObject() && event.getProperty("id", {}).toString() == activeId
            && event.getProperty("status", {}).toString().isNotEmpty())
            jobRunning = false;
        target = callback;
    }
    if (target)
        target(event);
}

void EngineClient::run()
{
    juce::String buffered;
    char block[4096];

    while (!threadShouldExit())
    {
        const auto count = ::read(outputDescriptor, block, sizeof(block));
        if (count < 0 && errno == EINTR)
            continue;
        if (threadShouldExit())
            return;
        if (count <= 0)
            break;

        buffered += juce::String::fromUTF8(block, static_cast<int>(count));
        int newline = 0;
        while ((newline = buffered.indexOfChar('\n')) >= 0)
        {
            const auto line = buffered.substring(0, newline).trim();
            buffered = buffered.substring(newline + 1);
            if (line.isEmpty())
                continue;

            const auto event = juce::JSON::parse(line);
            emit(event.isObject() ? event
                                  : failureEvent(activeJobId(), "Dettato Engine returned an invalid response."));
        }
    }

    if (!threadShouldExit())
        emit(failureEvent(activeJobId(), "Dettato Engine stopped unexpectedly. You can try the transcription again."));
}

void EngineClient::cancel()
{
    shutdown();
}

void EngineClient::shutdown()
{
    {
        const juce::ScopedLock sl(lock);
        activeId.clear();
        jobRunning = false;
        if (processId > 0)
            ::kill(processId, SIGTERM);
    }

    if (isThreadRunning() && !stopThread(2000))
    {
        if (processId > 0)
            ::kill(processId, SIGKILL);
        stopThread(10000);
    }

    const juce::ScopedLock sl(lock);
    closeDescriptor(inputDescriptor);
    closeDescriptor(outputDescriptor);
    if (processId > 0)
    {
        while (::waitpid(processId, nullptr, 0) < 0 && errno == EINTR)
        {
        }
    }
    processId = -1;
}
} // namespace dettato
