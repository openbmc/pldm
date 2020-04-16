#include "config.h"

#include "softoff.hpp"

#include <unistd.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <sdbusplus/bus.hpp>
#include <sdeventplus/event.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

int main()
{
    using namespace phosphor::logging;
    using namespace pldm;

    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    // Get a handle to system dbus.
    auto bus = sdbusplus::bus::new_default();

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

    // pldm::PldmSoftPowerOff  SoftPowerOff;
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
            log<level::ERR>("PLDM host soft off: Failure in processing request",
                            entry("ERROR=%s", e.what()));
            return 1;
        }
    }

    // Don't block chassis off target
    softPower.guardChassisOff(DISABLE_GUARD);

    // Now request the shutdown
    softPower.sendChassisOffcommand();

    return 0;
}
