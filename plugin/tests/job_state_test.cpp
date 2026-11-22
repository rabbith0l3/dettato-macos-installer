#include <cassert>
#include <iostream>

#include "JobState.h"

int main()
{
    dettato::JobState jobs;

    jobs.start("first");
    jobs.cancel();
    jobs.succeed("first", "/tmp/stale.mid", "stale");
    assert(jobs.snapshot().status == dettato::JobStatus::idle);

    jobs.start("second");
    jobs.succeed("second", "/tmp/current.mid", "ok");
    const auto current = jobs.snapshot();
    assert(current.status == dettato::JobStatus::succeeded);
    assert(current.midiPath == "/tmp/current.mid");
    assert(current.summary == "ok");

    jobs.start("third");
    jobs.fail("other", "stale failure");
    assert(jobs.snapshot().status == dettato::JobStatus::running);
    jobs.fail("third", "Engine could not decode the file");
    assert(jobs.snapshot().status == dettato::JobStatus::failed);
    assert(jobs.snapshot().error == "Engine could not decode the file");

    std::cout << "job state cancellation and stale-result protection passed\n";
}
