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
    mctpEndpointSignal(bus,
                       sdbusplus::bus::match::rules::interfacesAdded(
                           "/xyz/openbmc_project/mctp"),
                       std::bind_front(&MctpDiscovery::discoverEndpoints, this))
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

    MctpInfos mctpInfos;

    for (const auto& [objectPath, interfaces] : objects)
    {
        UUID uuid{};
        for (const auto& [intfName, properties] : interfaces)
        {

            if (intfName == uuidEndpointIntfName)
            {
                uuid = std::get<std::string>(properties.at("UUID"));
            }

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
                        mctpInfos.emplace_back(std::make_pair(eid, uuid));
                    }
                }
            }
        }
    }

    if (mctpInfos.size() && fwManager)
    {
        fwManager->handleMCTPEndpoints(mctpInfos);
    }
}

void MctpDiscovery::discoverEndpoints(sdbusplus::message::message& msg)
{
    MctpInfos mctpInfos;

    sdbusplus::message::object_path objPath;
    std::map<std::string, std::map<std::string, dbus::Value>> interfaces;
    msg.read(objPath, interfaces);

    UUID uuid{};
    for (const auto& [intfName, properties] : interfaces)
    {
        if (intfName == uuidEndpointIntfName)
        {
            uuid = std::get<std::string>(properties.at("UUID"));
        }

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
                    mctpInfos.emplace_back(std::make_pair(eid, uuid));
                }
            }
        }
    }

    if (mctpInfos.size() && fwManager)
    {
        fwManager->handleMCTPEndpoints(mctpInfos);
    }
}

} // namespace pldm