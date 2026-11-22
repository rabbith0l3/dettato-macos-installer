#pragma once

#include <mutex>
#include <string>

namespace dettato
{
enum class JobStatus { idle, running, succeeded, failed };

struct JobSnapshot
{
    JobStatus status { JobStatus::idle };
    std::string id;
    std::string midiPath;
    std::string summary;
    std::string error;
};

class JobState
{
public:
    void start(const std::string& id);
    void cancel();
    void succeed(const std::string& id, const std::string& midiPath, const std::string& summary);
    void fail(const std::string& id, const std::string& error);
    JobSnapshot snapshot() const;

private:
    mutable std::mutex mutex;
    JobSnapshot current;
};
} // namespace dettato
