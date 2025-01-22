#include <systemd/sd-event.h>

#include <sdeventplus/event.hpp>

namespace utils
{
void runEventLoopForSeconds(sdeventplus::Event& event, uint64_t sec)
{
    uint64_t t0 = 0;
    uint64_t t1 = 0;
    uint64_t usec = sec * 1000000;
    uint64_t elapsed = 0;
    sd_event_now(event.get(), CLOCK_MONOTONIC, &t0);
    do
    {
        if (!sd_event_run(event.get(), usec - elapsed))
        {
            break;
        }
        sd_event_now(event.get(), CLOCK_MONOTONIC, &t1);
        elapsed = t1 - t0;
    } while (elapsed < usec);
}
} // namespace utils
