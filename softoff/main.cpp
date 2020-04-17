#include "config.h"

#include "common/utils.hpp"
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

    // Time out or soft off complete
    while (!softPower.isCompleted() && !softPower.isTimerExpired())
    {
        try
        {
            event.run(std::nullopt);
        }
        catch (const sdeventplus::SdEventError& e)
        {
            std::cerr
                << "PLDM host soft off: Failure in processing request.ERROR= "
                << e.what() << "\n";
            return -1;
        }
    }

    if (softPower.isTimerExpired())
    {
        pldm::utils::reportError("xyz.openbmc_project.bmc.pldm.SoftOffTimeout");
        std::cerr
            << "PLDM host soft off: ERROR! Wait for the host soft off timeout."
            << "Exit the pldm-softpoweroff "
            << "\n";
        return -1;
    }

    return 0;
}
