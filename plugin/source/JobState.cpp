#include "JobState.h"

namespace dettato
{
void JobState::start(const std::string& id)
{
    const std::lock_guard lock(mutex);
    current = {};
    current.status = JobStatus::running;
    current.id = id;
}

void JobState::cancel()
{
    const std::lock_guard lock(mutex);
    current = {};
}

void JobState::succeed(const std::string& id, const std::string& midiPath, const std::string& summary)
{
    const std::lock_guard lock(mutex);
    if (current.status != JobStatus::running || current.id != id)
        return;

    current.status = JobStatus::succeeded;
    current.midiPath = midiPath;
    current.summary = summary;
}

void JobState::fail(const std::string& id, const std::string& error)
{
    const std::lock_guard lock(mutex);
    if (current.status != JobStatus::running || current.id != id)
        return;

    current.status = JobStatus::failed;
    current.error = error;
    current.midiPath.clear();
    current.summary.clear();
}

JobSnapshot JobState::snapshot() const
{
    const std::lock_guard lock(mutex);
    return current;
}
} // namespace dettato
