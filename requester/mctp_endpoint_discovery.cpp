#include "mctp_endpoint_discovery.hpp"

#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace pldm
{

MctpDiscovery::MctpDiscovery(sdbusplus::bus::bus& bus,
                             fw_update::Manager* fwManager) :
    bus(bus),
    fwManager(fwManager),
    mctpEndpointAddedSignal(bus,
                       sdbusplus::bus::match::rules::interfacesAdded(
                           "/xyz/openbmc_project/mctp"),
                       std::bind_front(&MctpDiscovery::dicoverEndpoints, this)),
    mctpEndpointRemovedSignal(bus,
                       sdbusplus::bus::match::rules::interfacesRemoved(
                           "/xyz/openbmc_project/mctp"),
                       std::bind_front(&MctpDiscovery::removeEndpoints, this))
{
    dbus::ObjectValueTree objects;

    try
    {
        auto method = bus.new_method_call(
            "xyz.openbmc_project.MCTP.Control", "/xyz/openbmc_project/mctp",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        auto reply = bus.call(method);
        reply.read(objects);
    }
    catch (const std::exception& e)
    {
        return;
    }

    std::vector<mctp_eid_t> eids;

    for (const auto& [objectPath, interfaces] : objects)
    {
        for (const auto& [intfName, properties] : interfaces)
        {
            if (intfName == mctpEndpointIntfName)
            {
                if (properties.contains("EID") &&
                    properties.contains("SupportedMessageTypes"))
                {
                    auto eid = std::get<size_t>(properties.at("EID"));
                    auto types = std::get<std::vector<uint8_t>>(
                        properties.at("SupportedMessageTypes"));
                    if (std::find(types.begin(), types.end(), mctpTypePLDM) !=
                        types.end())
                    {
                        eids.emplace_back(eid);
                    }
                }
            }
        }
    }

    if (eids.size() && fwManager)
    {
        fwManager->handleMCTPEndpoints(eids);
    }
}

void MctpDiscovery::dicoverEndpoints(sdbusplus::message::message& msg)
{
    constexpr std::string_view mctpEndpointIntfName{
        "xyz.openbmc_project.MCTP.Endpoint"};
    std::vector<mctp_eid_t> eids;

    sdbusplus::message::object_path objPath;
    std::map<std::string, std::map<std::string, dbus::Value>> interfaces;
    msg.read(objPath, interfaces);

    for (const auto& [intfName, properties] : interfaces)
    {
        if (intfName == mctpEndpointIntfName)
        {
            if (properties.contains("EID") &&
                properties.contains("SupportedMessageTypes"))
            {
                auto eid = std::get<size_t>(properties.at("EID"));
                auto types = std::get<std::vector<uint8_t>>(
                    properties.at("SupportedMessageTypes"));
                if (std::find(types.begin(), types.end(), mctpTypePLDM) !=
                    types.end())
                {
                    eids.emplace_back(eid);
                }
            }
        }
    }

    if (eids.size() && fwManager)
    {
        fwManager->handleMCTPEndpoints(eids);
    }
}

void MctpDiscovery::removeEndpoints(sdbusplus::message::message& msg)
{
    constexpr std::string_view mctpEndpointIntfName{
        "xyz.openbmc_project.MCTP.Endpoint"};
    std::vector<mctp_eid_t> eids;

    sdbusplus::message::object_path objPath;
    std::map<std::string, std::map<std::string, dbus::Value>> interfaces;
    msg.read(objPath, interfaces);

    for (const auto& [intfName, properties] : interfaces)
    {
        if (intfName == mctpEndpointIntfName)
        {
            if (properties.contains("EID") &&
                properties.contains("SupportedMessageTypes"))
            {
                auto eid = std::get<size_t>(properties.at("EID"));
                auto types = std::get<std::vector<uint8_t>>(
                    properties.at("SupportedMessageTypes"));
                if (std::find(types.begin(), types.end(), mctpTypePLDM) !=
                    types.end())
                {
                    eids.emplace_back(eid);
                }
            }
        }
    }

    /* Do somethings with the removed MCTP endpoints */
}

} // namespace pldm