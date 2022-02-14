#include "config.h"

#include "mctp_endpoint_discovery.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace pldm
{
const std::string emptyUUID = "00000000-0000-0000-0000-000000000000";
// Required strings for sending the msg to check on host
constexpr auto MCTP_INTERFACE = "xyz.openbmc_project.MCTP.Endpoint";
constexpr auto MCTP_OBJECT_PATH = "/xyz/openbmc_project/mctp";

MctpDiscovery::MctpDiscovery(
    sdbusplus::bus::bus& bus,
    std::initializer_list<MctpDiscoveryHandlerIntf*> list) :
    bus(bus),
    mctpEndpointAddedSignal(
        bus, sdbusplus::bus::match::rules::interfacesAdded(MCTP_OBJECT_PATH),
        std::bind_front(&MctpDiscovery::discoverEndpoints, this)),
    mctpEndpointRemovedSignal(
        bus, sdbusplus::bus::match::rules::interfacesRemoved(MCTP_OBJECT_PATH),
        std::bind_front(&MctpDiscovery::removeEndpoints, this)),
    handlers(list)
{
    getMctpInfos(exisingMctpInfos);
    handleMctpEndpoints(exisingMctpInfos);
}

void MctpDiscovery::getMctpInfos(MctpInfos& mctpInfos)
{
    // Find all implementations of the MCTP Endpoint interface
    pldm::utils::GetSubTreeResponse mapperResponse;
    try
    {
        mapperResponse = pldm::utils::DBusHandler().getSubtree(
            MCTP_OBJECT_PATH, 0, std::vector<std::string>({MCTP_INTERFACE}));
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            " getSubtree call failed with, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
            "ERROR", e.what(), "PATH", MCTP_OBJECT_PATH, "INTERFACE",
            MCTP_INTERFACE);
        return;
    }

    for (const auto& [path, services] : mapperResponse)
    {
        for (const auto& serviceIter : services)
        {
            const std::string& service = serviceIter.first;
            try
            {
                auto properties =
                    pldm::utils::DBusHandler().getDbusPropertiesVariant(
                        service.c_str(), path.c_str(), MCTP_INTERFACE);

                if (properties.contains("NetworkId") &&
                    properties.contains("EID") &&
                    properties.contains("SupportedMessageTypes"))
                {
                    auto networkId =
                        std::get<NetworkId>(properties.at("NetworkId"));
                    auto eid = std::get<mctp_eid_t>(properties.at("EID"));
                    auto types = std::get<std::vector<uint8_t>>(
                        properties.at("SupportedMessageTypes"));
                    if (std::find(types.begin(), types.end(), mctpTypePLDM) !=
                        types.end())
                    {
                        info("Adding Endpoint networkId={NETWORK} EID={EID}",
                             "NETWORK", networkId, "EID", unsigned(eid));
                        mctpInfos.emplace_back(
                            MctpInfo(eid, emptyUUID, "", networkId));
                    }
                }
            }
            catch (const sdbusplus::exception_t& e)
            {
                error(
                    "Error reading MCTP Endpoint property, ERROR={ERROR} SERVICE={SERVICE} PATH={PATH}",
                    "ERROR", e.what(), "SERVICE", service, "PATH", path);
                return;
            }
        }
    }
}

void MctpDiscovery::getAddedMctpInfos(sdbusplus::message_t& msg,
                                      MctpInfos& mctpInfos)
{
    sdbusplus::message::object_path objPath;
    using Property = std::string;
    using PropertyMap = std::map<Property, dbus::Value>;
    std::map<std::string, PropertyMap> interfaces;
    try
    {
        msg.read(objPath, interfaces);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Error reading MCTP Endpoint addedInterace message, ERROR={ERROR}",
            "ERROR", e.what());
        return;
    }

    for (const auto& [intfName, properties] : interfaces)
    {
        if (intfName == MCTP_INTERFACE)
        {
            if (properties.contains("NetworkId") &&
                properties.contains("EID") &&
                properties.contains("SupportedMessageTypes"))
            {
                auto networkId =
                    std::get<NetworkId>(properties.at("NetworkId"));
                auto eid = std::get<mctp_eid_t>(properties.at("EID"));
                auto types = std::get<std::vector<uint8_t>>(
                    properties.at("SupportedMessageTypes"));
                if (std::find(types.begin(), types.end(), mctpTypePLDM) !=
                    types.end())
                {
                    info("Adding Endpoint networkId={NETWORK} EID={EID}",
                         "NETWORK", networkId, "EID", unsigned(eid));
                    mctpInfos.emplace_back(
                        MctpInfo(eid, emptyUUID, "", networkId));
                }
            }
        }
    }
}

void MctpDiscovery::addToExistingMctpInfos(const MctpInfos& addedInfos)
{
    for (const auto& mctpInfo : addedInfos)
    {
        if (std::find(exisingMctpInfos.begin(), exisingMctpInfos.end(),
                      mctpInfo) == exisingMctpInfos.end())
        {
            exisingMctpInfos.emplace_back(mctpInfo);
        }
    }
}

void MctpDiscovery::removeFromExistingMctpInfos(MctpInfos& mctpInfos,
                                                MctpInfos& removedInfos)
{
    for (const auto& mctpInfo : exisingMctpInfos)
    {
        if (std::find(mctpInfos.begin(), mctpInfos.end(), mctpInfo) ==
            mctpInfos.end())
        {
            removedInfos.emplace_back(mctpInfo);
        }
    }
    for (const auto& mctpInfo : removedInfos)
    {
        info("Removing Endpoint networkId={NETWORK} EID={EID}", "NETWORK",
             std::get<3>(mctpInfo), "EID", unsigned(std::get<0>(mctpInfo)));
        exisingMctpInfos.erase(std::remove(exisingMctpInfos.begin(),
                                           exisingMctpInfos.end(), mctpInfo),
                               exisingMctpInfos.end());
    }
}

void MctpDiscovery::discoverEndpoints(sdbusplus::message_t& msg)
{
    MctpInfos addedInfos;
    getAddedMctpInfos(msg, addedInfos);
    addToExistingMctpInfos(addedInfos);
    handleMctpEndpoints(addedInfos);
}

void MctpDiscovery::removeEndpoints([[maybe_unused]] sdbusplus::message_t& msg)
{
    MctpInfos mctpInfos;
    MctpInfos removedInfos;
    getMctpInfos(mctpInfos);
    removeFromExistingMctpInfos(mctpInfos, removedInfos);
    handleRemovedMctpEndpoints(removedInfos);
}

void MctpDiscovery::handleMctpEndpoints(const MctpInfos& mctpInfos)
{
    for (const auto& handler : handlers)
    {
        if (handler)
        {
            handler->handleMctpEndpoints(mctpInfos);
        }
    }
}

void MctpDiscovery::handleRemovedMctpEndpoints(const MctpInfos& mctpInfos)
{
    for (const auto& handler : handlers)
    {
        if (handler)
        {
            handler->handleRemovedMctpEndpoints(mctpInfos);
        }
    }
}

} // namespace pldm
