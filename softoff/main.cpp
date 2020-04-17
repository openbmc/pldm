#include "config.h"

#include "softoff.hpp"

#include <sdeventplus/event.hpp>

int main()
{

    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    // Get a handle to system dbus.
    auto bus = sdbusplus::bus::new_default();

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

    pldm::PldmSoftPowerOff softPower = pldm::PldmSoftPowerOff(bus, event.get());

    // Block chassis off target
    softPower.guardChassisOff(ENABLE_GUARD);

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
            return 1;
        }
    }

    // Don't block chassis off target
    softPower.guardChassisOff(DISABLE_GUARD);

    // Now request the shutdown
    softPower.sendChassisOffcommand();

    return 0;
}
