#include "common/utils.hpp"
#include "softoff.hpp"

#include <phosphor-logging/lg2.hpp>

#include <iostream>

PHOSPHOR_LOG2_USING;

int main()
{
    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    // Get a handle to system D-Bus.
    auto& bus = pldm::utils::DBusHandler::getBus();

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

    pldm::SoftPowerOff softPower(bus, event.get());

    if (softPower.isError())
    {
        error(
            "Host failed to gracefully shutdown, exiting pldm-softpoweroff app");
        return -1;
    }

    if (softPower.isCompleted())
    {
        error(
            "Host current state is not Running, exiting pldm-softpoweroff app");
        return 0;
    }

    // Send the gracefully shutdown request to the host and
    // wait the host gracefully shutdown.
    if (softPower.hostSoftOff(event))
    {
        error(
            "pldm-softpoweroff:Failure in sending soft off request to the host. Exiting pldm-softpoweroff app");
        return -1;
    }

    if (softPower.isTimerExpired() && softPower.isReceiveResponse())
    {
        pldm::utils::reportError(
            "pldm soft off: Waiting for the host soft off timeout");

        auto method = bus.new_method_call(
            "xyz.openbmc_project.Dump.Manager", "/xyz/openbmc_project/dump/bmc",
            "xyz.openbmc_project.Dump.Create", "CreateDump");
        method.append(
            std::vector<
                std::pair<std::string, std::variant<std::string, uint64_t>>>());
        try
        {
            bus.call_noreply(method);
        }
        catch (const sdbusplus::exception::exception& e)
        {
            std::cerr << "SoftPowerOff:Failed to create BMC dump, ERROR="
                      << e.what() << std::endl;
        }

        error(
            "PLDM host soft off: ERROR! Wait for the host soft off timeout. Exit the pldm-softpoweroff");
        return -1;
    }

    return 0;
}
