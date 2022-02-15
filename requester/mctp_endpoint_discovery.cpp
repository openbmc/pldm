#include "mctp_endpoint_discovery.hpp"

#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "common/utils.hpp"
#include "requester/terminus_manager.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace pldm
{

MctpDiscovery::MctpDiscovery(sdbusplus::bus::bus& bus,
                             fw_update::Manager* fwManager,
                             terminus::Manager* devManager) :
    bus(bus),
    fwManager(fwManager), devManager(devManager),
    mctpEndpointAddedSignal(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/mctp"),
        std::bind_front(&MctpDiscovery::discoverEndpoints, this)),
    mctpEndpointRemovedSignal(
        bus,
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

    if (!objects.size())
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

    /* Initial the listEids with the end points in MCTP D-Bus interface */
    listEids = eids;

    if (eids.size() && fwManager)
    {
        fwManager->handleMCTPEndpoints(eids);
    }

    if (eids.size() && devManager)
    {
        devManager->addDevices(eids);
    }
}

void MctpDiscovery::discoverEndpoints(sdbusplus::message::message& msg)
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
                    /* Add eid to list Endpoints */
                    if (!std::count(listEids.begin(), listEids.end(), eid))
                    {
                        listEids.emplace_back(eid);
                    }
                }
            }
        }
    }

    if (eids.size() && fwManager)
    {
        fwManager->handleMCTPEndpoints(eids);
    }

    if (eids.size() && devManager)
    {
        devManager->addDevices(eids);
    }
}

void MctpDiscovery::removeEndpoints(sdbusplus::message::message& msg)
{
    dbus::ObjectValueTree objects;
    /* Unused */
    msg = msg;

    /*
     * interfaceRemoved signal does not include the removed path in the response
     * message. Check the remained EID in the MCTP D-Bus interface and compare
     * with the previous list to find the removed EIDs
     */
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
        if (listEids.size() && devManager)
        {
            devManager->removeDevices(listEids);
        }
        /* Remove all of EID in list when MCTP D-Bus is empty*/
        listEids = {};
        return;
    }

    if (!objects.size())
    {
        if (listEids.size() && devManager)
        {
            devManager->removeDevices(listEids);
        }
        /* Remove all of EID in list when MCTP D-Bus is empty*/
        listEids = {};
        return;
    }

    /* All of EIDs in MCTP D-Bus is removed */
    if (!listEids.size())
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

    /* Find the removed EID */
    std::sort(listEids.begin(), listEids.end());
    std::sort(eids.begin(), eids.end());
    std::vector<mctp_eid_t> difference;
    std::set_difference(listEids.begin(), listEids.end(), eids.begin(),
                        eids.end(), std::back_inserter(difference));

    /* Update the list EIDs with the current endpoints in MCTP D-Bus */
    listEids = eids;
    if (difference.size() && devManager)
    {
        devManager->removeDevices(difference);
    }

    return;
}

} // namespace pldm