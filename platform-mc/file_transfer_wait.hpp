#pragma once

#include <systemd/sd-event.h>

#include <stdexec/execution.hpp>

#include <cerrno>
#include <optional>
#include <utility>

namespace pldm::platform_mc::file_transfer
{
/** Yield to the owning event loop. No poll/sleep blocks requester timers.
 * This short timer is a conservative adapter for the existing exec::task API;
 * a readiness sender can replace it without changing transfer semantics.
 */
struct Yield
{
    using sender_concept = stdexec::sender_t;
    template <typename... Env>
    auto get_completion_signatures(Env&&...) const
        -> stdexec::completion_signatures<stdexec::set_value_t(int),
                                          stdexec::set_stopped_t()>;

    template <stdexec::receiver R>
    struct Operation
    {
        R receiver;
        sd_event* event = nullptr;
        sd_event_source* timer = nullptr;
        struct Stop
        {
            Operation* operation;
            void operator()() const noexcept
            {
                operation->disarm();
                stdexec::set_stopped(std::move(operation->receiver));
            }
        };
        std::optional<typename stdexec::stop_token_of_t<
            stdexec::env_of_t<R>>::template callback_type<Stop>>
            stop;

        explicit Operation(R r) : receiver(std::move(r)) {}
        Operation(const Operation&) = delete;
        ~Operation()
        {
            sd_event_source_unref(timer);
            sd_event_unref(event);
        }
        void disarm()
        {
            sd_event_source_unref(std::exchange(timer, nullptr));
        }
        static int ready(sd_event_source*, uint64_t, void* context)
        {
            auto* self = static_cast<Operation*>(context);
            self->disarm();
            self->stop.reset();
            stdexec::set_value(std::move(self->receiver), 0);
            return 0;
        }
        void start() noexcept
        {
            auto token = stdexec::get_stop_token(stdexec::get_env(receiver));
            if (token.stop_requested())
            {
                stdexec::set_stopped(std::move(receiver));
                return;
            }
            auto rc = sd_event_default(&event);
            if (rc >= 0)
            {
                rc = sd_event_add_time_relative(event, &timer, CLOCK_MONOTONIC,
                                                1000, 1, ready, this);
            }
            if (rc < 0)
            {
                disarm();
                stdexec::set_value(std::move(receiver), rc);
                return;
            }
            if (token.stop_possible())
            {
                stop.emplace(token, Stop{this});
            }
        }
    };
    template <stdexec::receiver R>
    auto connect(R receiver) const
    {
        return Operation<R>(std::move(receiver));
    }
};
} // namespace pldm::platform_mc::file_transfer
