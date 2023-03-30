#include "config.h"

#include "mctp_endpoint_discovery.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/pldm.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace pldm
{
const std::string emptyUUID = "00000000-0000-0000-0000-000000000000";

MctpDiscovery::MctpDiscovery(
    sdbusplus::bus::bus& bus,
    std::initializer_list<MctpDiscoveryHandlerIntf*> list,
    const std::filesystem::path& staticEidTablePath) :
    bus(bus),
    mctpEndpointAddedSignal(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/mctp"),
        std::bind_front(&MctpDiscovery::discoverEndpoints, this)),
    mctpEndpointRemovedSignal(
        bus,
        sdbusplus::bus::match::rules::interfacesRemoved(
            "/xyz/openbmc_project/mctp"),
        std::bind_front(&MctpDiscovery::discoverEndpoints, this)),
    handlers(list), staticEidTablePath(staticEidTablePath)
{
    MctpInfos mctpInfos;
    loadEidToMCTPMediumConfigs();
    getMctpInfos(mctpInfos);
    loadStaticEndpoints(mctpInfos);
    handleMctpEndpoints(mctpInfos);
}

void MctpDiscovery::getMctpInfos(MctpInfos& mctpInfos)
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

    for (const auto& [objectPath, interfaces] : objects)
    {
        for (const auto& [intfName, properties] : interfaces)
        {
            if (intfName == mctpEndpointIntfName)
            {
                if (properties.contains("NetworkId") &&
                    properties.contains("EID") &&
                    properties.contains("SupportedMessageTypes"))
                {
                    auto networkId =
                        std::get<size_t>(properties.at("NetworkId"));
                    auto eid = std::get<size_t>(properties.at("EID"));
                    auto types = std::get<std::vector<uint8_t>>(
                        properties.at("SupportedMessageTypes"));
                    MctpMedium mctpMediumStr = "";
                    if (eidToMctpMediums.find(eid) != eidToMctpMediums.end())
                    {
                        mctpMediumStr = eidToMctpMediums[eid];
                    }
                    if (std::find(types.begin(), types.end(), mctpTypePLDM) !=
                        types.end())
                    {
                        mctpInfos.emplace_back(
                            MctpInfo(eid, emptyUUID, mctpMediumStr, networkId));
                    }
                }
            }
        }
    }
}

void MctpDiscovery::discoverEndpoints(
    [[maybe_unused]] sdbusplus::message_t& msg)
{
    MctpInfos mctpInfos;
    loadEidToMCTPMediumConfigs();
    getMctpInfos(mctpInfos);
    loadStaticEndpoints(mctpInfos);
    handleMctpEndpoints(mctpInfos);
}

void MctpDiscovery::loadEidToMCTPMediumConfigs()
{
    if (!std::filesystem::exists(EID_TO_MCTP_MEDIUM))
    {
        std::cerr << EID_TO_MCTP_MEDIUM << "is not existing." << std::endl;
        return;
    }

    static const std::vector<nlohmann::json> emptyJsonArray{};
    std::ifstream jsonFile(EID_TO_MCTP_MEDIUM);
    auto datas = nlohmann::json::parse(jsonFile, nullptr, false);
    if (datas.is_discarded())
    {
        std::cerr << "Parsing json file failed, FILE=" << EID_TO_MCTP_MEDIUM
                  << "\n";
        return;
    }

    auto entries = datas.value("eids", emptyJsonArray);
    for (const auto& entry : entries)
    {
        auto eid = entry.value("eid", -1);
        if (eid == -1)
        {
            std::cerr << "Invalid \"eid\" configuration" << std::endl;
            continue;
        }
        auto mapString = entry.value("string", "");
        if (mapString == "")
        {
            std::cerr << "Invalid configuration of \"string\" of eid "
                      << unsigned(eid) << std::endl;
            continue;
        }
        eidToMctpMediums[eid] = mapString;
    }

    return;
}

void MctpDiscovery::loadStaticEndpoints(MctpInfos& mctpInfos)
{
    if (!std::filesystem::exists(staticEidTablePath))
    {
        return;
    }

    std::ifstream jsonFile(staticEidTablePath);
    auto data = nlohmann::json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        std::cerr << "Parsing json file failed, FILE=" << staticEidTablePath
                  << "\n";
        return;
    }

    const std::vector<nlohmann::json> emptyJsonArray{};
    auto endpoints = data.value("Endpoints", emptyJsonArray);
    for (const auto& endpoint : endpoints)
    {
        const std::vector<uint8_t> emptyUnit8Array;
        auto networkId = endpoint.value("NetworkId", 0xFF);
        auto eid = endpoint.value("EID", 0xFF);
        auto types = endpoint.value("SupportedMessageTypes", emptyUnit8Array);
        if (std::find(types.begin(), types.end(), mctpTypePLDM) != types.end())
        {
            MctpInfo mctpInfo(eid, emptyUUID, "", networkId);
            if (std::find(mctpInfos.begin(), mctpInfos.end(), mctpInfo) ==
                mctpInfos.end())
            {
                mctpInfos.emplace_back(mctpInfo);
            }
            else
            {
                std::cerr
                    << "Found EID(" << unsigned(eid)
                    << ") in static eid table duplicating to MCTP D-Bus interface\n";
            }
        }
    }
}

void MctpDiscovery::handleMctpEndpoints(const MctpInfos& mctpInfos)
{
    for (MctpDiscoveryHandlerIntf* handler : handlers)
    {
        if (handler)
        {
            handler->handleMctpEndpoints(mctpInfos);
        }
    }
}

} // namespace pldm
