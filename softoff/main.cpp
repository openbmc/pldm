#include "config.h"

#include "softoff.hpp"

#include <iostream>
#include <sdeventplus/event.hpp>

int main()
{
    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    // Get a handle to system dbus.
    auto bus = sdbusplus::bus::new_default();

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

    pldm::SoftPowerOff softPower(bus, event.get());

    if (softPower.isHasError() == true)
    {
        std::cerr << "Host failed to gracefully shutdown,exit the "
                     "pldm-softpoweroff\n";
        return -1;
    }

    return 0;
}
