#include "config.h"

#include "softoff.hpp"

#include <sdeventplus/event.hpp>

#include <iostream>

int main()
{
    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    // Get a handle to system D-Bus.
    auto bus = sdbusplus::bus::new_default();

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

    pldm::SoftPowerOff softPower(bus, event.get());

    if (softPower.isError() == true)
    {
        std::cerr << "Host failed to gracefully shutdown, exiting "
                     "pldm-softpoweroff app\n";
        return -1;
    }

    if (softPower.isCompleted() == true)
    {
        return 0;
    }

    return 0;
}
