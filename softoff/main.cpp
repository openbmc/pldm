#include "config.h"

#include "softoff.hpp"

#include <sdeventplus/event.hpp>

#include "libpldm/base.h"

int main()
{

    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    // Get a handle to system dbus.
    auto bus = sdbusplus::bus::new_default();

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

    pldm::PldmSoftPowerOff softPower = pldm::PldmSoftPowerOff(bus, event.get());

    if (softPower.isHasError() == true)
    {
        std::cerr << "Exit the pldm-softpoweroff\n";
        return PLDM_ERROR;
    }

    // Time out or soft off complete
    while (!softPower.iscompleted() && !softPower.isTimerExpired())
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
            return PLDM_ERROR;
        }
    }

    if (softPower.isTimerExpired())
    {
        std::cerr
            << "PLDM host soft off: ERROR! Wait for the host soft off timeout."
            << "Exit the pldm-softpoweroff "
            << "\n";
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}
