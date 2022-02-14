#include "mctp_endpoint_discovery.hpp"

#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace pldm
{

MctpDiscovery::MctpDiscovery(
    sdbusplus::bus::bus& bus,
    std::initializer_list<IMctpDiscoveryHandler*> list) :
    bus(bus),
    mctpEndpointSignal(bus,
                       sdbusplus::bus::match::rules::interfacesAdded(
                           "/xyz/openbmc_project/mctp"),
                       std::bind_front(&MctpDiscovery::dicoverEndpoints, this))
{
    dbus::ObjectValueTree objects;

    for (auto elem : list)
    {
        handlers.emplace_back(elem);
    }

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

    if (eids.size() && handlers.size())
    {
        for (IMctpDiscoveryHandler* h : handlers)
        {
            if (h)
            {
                h->handleMCTPEndpoints(eids);
            }
        }
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

    if (eids.size() && handlers.size())
    {
        for (IMctpDiscoveryHandler* h : handlers)
        {
            if (h)
            {
                h->handleMCTPEndpoints(eids);
            }
        }
    }
}

void MctpDiscovery::loadStaticEndpoints(const std::string& jsonPath)
{
    std::ifstream jsonFile(jsonPath);
    if (!jsonFile.good())
    {
        std::cerr << "Could not open static EIDs file: " << jsonPath << "\n";
        return;
    }

    auto data = nlohmann::json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        std::cerr << "Parsing json file failed, FILE=" << jsonPath << std::endl;
        return;
    }

    std::vector<mctp_eid_t> eids;
    const nlohmann::json emptyJson{};
    const std::vector<nlohmann::json> emptyJsonArray{};
    auto endpoints = data.value("endpoints", emptyJsonArray);
    for (const auto& endpoint : endpoints)
    {
        const std::vector<uint8_t> emptyUnit8Array;
        auto eid = endpoint.value("EID", 0xFF);
        auto types = endpoint.value("SupportedMessageTypes", emptyUnit8Array);
        if (std::find(types.begin(), types.end(), mctpTypePLDM) != types.end())
        {
            eids.emplace_back(eid);
        }
    }

    if (!eids.size())
    {
        std::cerr << "No EID defined in json file supports PLDM" << std::endl;
        return;
    }

    if (handlers.size())
    {
        for (IMctpDiscoveryHandler* h : handlers)
        {
            if (h)
            {
                h->handleMCTPEndpoints(eids);
            }
        }
    }

    return;
}

} // namespace pldm