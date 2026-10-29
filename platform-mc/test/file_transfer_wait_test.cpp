#include "../file_transfer_wait.hpp"

#include <exec/async_scope.hpp>
#include <exec/task.hpp>

#include <iostream>
#include <stdexcept>

using pldm::platform_mc::file_transfer::Yield;

static void require(bool condition)
{
    if (!condition)
    {
        throw std::runtime_error("event-loop yield check failed");
    }
}

static exec::task<int> yieldRepeatedly()
{
    for (int i = 0; i < 100; ++i)
    {
        if (auto rc = co_await Yield{})
        {
            co_return rc;
        }
    }
    co_return 0;
}

int main()
{
    sd_event* event = nullptr;
    require(sd_event_default(&event) >= 0);
    {
        exec::async_scope scope;
        int result = -1;
        scope.spawn(
            yieldRepeatedly() | stdexec::then([&](int rc) {
                result = rc;
                sd_event_exit(event, 0);
            }),
            exec::default_task_context<void>(stdexec::inline_scheduler{}));
        require(sd_event_loop(event) >= 0);
        stdexec::sync_wait(scope.on_empty());
        require(result == 0);
    }
    sd_event_unref(event);
    event = nullptr;
    require(sd_event_default(&event) >= 0);
    {
        exec::async_scope scope;
        bool stopped = false;
        sd_event_source* cancel = nullptr;
        require(
            sd_event_add_time_relative(
                event, &cancel, CLOCK_MONOTONIC, 100, 1,
                [](sd_event_source*, uint64_t, void* context) {
                    static_cast<exec::async_scope*>(context)->request_stop();
                    return 0;
                },
                &scope) >= 0);
        scope.spawn(Yield{} |
                    stdexec::then([&](int) { sd_event_exit(event, 1); }) |
                    stdexec::upon_stopped([&] {
                        stopped = true;
                        sd_event_exit(event, 0);
                    }));
        require(sd_event_loop(event) == 0);
        stdexec::sync_wait(scope.on_empty());
        require(stopped);
        sd_event_source_unref(cancel);
    }
    sd_event_unref(event);
    std::cout << "100 event-loop yields and pending cancellation passed\n";
}
