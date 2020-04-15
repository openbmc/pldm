#include "dbus_to_host_effecters.hpp"

#include "utils.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/State/OperatingSystem/Status/server.hpp>

#include "libpldm/pdr.h"
#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

namespace pldm
{
namespace host_effecters
{

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr auto hostEffecterJson = "dbus_to_host_effecter.json";

void HostEffecterParser::populatePropVals(
    const Json& dBusValues, std::vector<PropertyValue>& propertyValues,
    const std::string& propertyType)

{
    for (auto it = dBusValues.begin(); it != dBusValues.end(); ++it)
    {
        auto value = jsonEntryToDbusVal(propertyType, it.value());
        propertyValues.push_back(value);
    }
}

void HostEffecterParser::parseEffecterJson(const std::string& jsonPath)
{
    fs::path jsonDir(jsonPath);
    if (!fs::exists(jsonDir) || fs::is_empty(jsonDir))
    {
        std::cerr << "Host Effecter json path does not exist, DIR=" << jsonPath
                  << "\n";
        return;
    }

    fs::path jsonFilePath = jsonDir / hostEffecterJson;
    if (!fs::exists(jsonFilePath))
    {
        std::cerr << "Host Effecter json does not exist, PATH=" << jsonFilePath
                  << "\n";
        throw InternalFailure();
    }

    const Json empty{};
    const std::vector<Json> emptyJsonList{};
    std::ifstream jsonFile(jsonFilePath);

    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        std::cerr << "Parsing Host Effecter json file failed, FILE="
                  << jsonFilePath << "\n";
        throw InternalFailure();
    }

    auto entries = data.value("entries", emptyJsonList);
    for (const auto& entry : entries)
    {
        EffecterInfo effecterInfo;
        effecterInfo.mctpEid = entry.value("mctp_eid", 0xFF);
        auto jsonEffecterInfo = entry.value("effecter_info", empty);
        auto effecterId =
            jsonEffecterInfo.value("effecterID", PLDM_INVALID_EFFECTER_ID);
        effecterInfo.containerId = jsonEffecterInfo.value("containerID", 0);
        effecterInfo.entityType = jsonEffecterInfo.value("entityType", 0);
        effecterInfo.entityInstance =
            jsonEffecterInfo.value("entityInstance", 0);
        effecterInfo.compEffecterCnt =
            jsonEffecterInfo.value("compositeEffecterCount", 0);
        auto effecters = entry.value("effecters", emptyJsonList);
        for (const auto& effecter : effecters)
        {
            DBusInfo dbusInfo{};
            auto jsonDbusInfo = effecter.value("dbus_info", empty);
            dbusInfo.dbusMap.objectPath = jsonDbusInfo.value("object_path", "");
            dbusInfo.dbusMap.interface = jsonDbusInfo.value("interface", "");
            dbusInfo.dbusMap.propertyName =
                jsonDbusInfo.value("property_name", "");
            dbusInfo.dbusMap.propertyType =
                jsonDbusInfo.value("property_type", "");
            Json propertyValues = jsonDbusInfo["property_values"];

            populatePropVals(propertyValues, dbusInfo.propertyValues,
                             dbusInfo.dbusMap.propertyType);

            const std::vector<uint8_t> emptyStates{};
            auto state = effecter.value("state", empty);
            dbusInfo.state.stateSetId = state.value("id", 0);
            auto states = state.value("states", emptyStates);
            if (dbusInfo.propertyValues.size() != states.size())
            {
                std::cerr << "Number of states do not match with"
                          << " number of D-Bus property values in the json. "
                          << "Object path " << dbusInfo.dbusMap.objectPath
                          << " and property " << dbusInfo.dbusMap.propertyName
                          << " will not be monitored \n";
                continue;
            }
            for (const auto& s : states)
            {
                dbusInfo.state.states.push_back(s);
            }

            auto effecterInfoIndex = hostEffecterInfo.size();
            auto dbusInfoIndex = effecterInfo.dbusInfo.size();
            using namespace sdbusplus::bus::match::rules;
            effecterInfoMatch.push_back(
                std::make_unique<sdbusplus::bus::match::match>(
                    pldm::utils::DBusHandler::getBus(),
                    propertiesChanged(dbusInfo.dbusMap.objectPath,
                                      dbusInfo.dbusMap.interface),
                    [this, effecterInfoIndex, dbusInfoIndex,
                     effecterId](sdbusplus::message::message& msg) {
                        DbusChgHostEffecterProps props;
                        std::string iface;
                        msg.read(iface, props);
                        processHostEffecterChangeNotification(
                            props, effecterInfoIndex, dbusInfoIndex,
                            effecterId);
                    }));

            effecterInfo.dbusInfo.push_back(std::move(dbusInfo));
        }
        hostEffecterInfo.push_back(std::move(effecterInfo));
    }
}

void HostEffecterParser::processHostEffecterChangeNotification(
    const DbusChgHostEffecterProps& chProperties, size_t effecterInfoIndex,
    size_t dbusInfoIndex, uint16_t effecterId)
{
    const auto& propertyName = hostEffecterInfo[effecterInfoIndex]
                                   .dbusInfo[dbusInfoIndex]
                                   .dbusMap.propertyName;

    const auto& it = chProperties.find(propertyName);

    if (it == chProperties.end())
    {
        return;
    }

    if (effecterId == PLDM_INVALID_EFFECTER_ID)
    {
        uint8_t* pdrData = nullptr;
        uint32_t pdrSize{};
        auto record = pldm_pdr_find_record_by_type(
            pdrRepo, PLDM_STATE_EFFECTER_PDR, NULL, &pdrData, &pdrSize);

        while (record)
        {
            auto pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrData);
            if (hostEffecterInfo[effecterInfoIndex].entityType ==
                    pdr->entity_type &&
                hostEffecterInfo[effecterInfoIndex].entityInstance ==
                    pdr->entity_instance &&
                hostEffecterInfo[effecterInfoIndex].containerId ==
                    pdr->container_id &&
                hostEffecterInfo[effecterInfoIndex].compEffecterCnt ==
                    pdr->composite_effecter_count)
            {
                effecterId = pdr->effecter_id;
                break;
            }
            pdrData = nullptr;
            record = pldm_pdr_find_record_by_type(
                pdrRepo, PLDM_STATE_EFFECTER_PDR, record, &pdrData, &pdrSize);
        }
    }
    constexpr auto hostStateInterface =
        "xyz.openbmc_project.State.OperatingSystem.Status";
    constexpr auto hostStatePath = "/xyz/openbmc_project/state/host0";

    try
    {
        using namespace sdbusplus::xyz::openbmc_project::State::
            OperatingSystem::server;
        auto currHostState =
            pldm::utils::DBusHandler().getDbusProperty<std::string>(
                hostStatePath, "OperatingSystemState", hostStateInterface);
        if ((currHostState != "xyz.openbmc_project.State.OperatingSystem."
                              "Status.OSStatus.Standby") &&
            (currHostState != "xyz.openbmc_project.State.OperatingSystem."
                              "Status.OSStatus.BootComplete"))
        {
            std::cout << "Host is not up. Current host state: "
                      << currHostState.c_str() << "\n";
            return;
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << "Error in getting current host state. Will still "
                     "continue to set the host effecter \n";
    }
    StateValue newState{};
    try
    {
        newState =
            findNewStateValue(effecterInfoIndex, dbusInfoIndex, it->second);
    }
    catch (const std::out_of_range& e)
    {
        std::cerr << "new state not found in json"
                  << "\n";
        return;
    }

    std::vector<set_effecter_state_field> stateField;
    for (uint8_t i = 0; i < hostEffecterInfo[effecterInfoIndex].compEffecterCnt;
         i++)
    {
        if (i == dbusInfoIndex)
        {
            stateField.push_back({PLDM_REQUEST_SET, newState});
        }
        else
        {
            stateField.push_back({PLDM_NO_CHANGE, 0});
        }
    }
    try
    {
        setHostStateEffecter(effecterInfoIndex, stateField, effecterId);
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Could not set host state effecter \n";
        return;
    }
}

StateValue
    HostEffecterParser::findNewStateValue(size_t effecterInfoIndex,
                                          size_t dbusInfoIndex,
                                          const PropertyValue& propertyValue)
{
    const auto& propValues = hostEffecterInfo[effecterInfoIndex]
                                 .dbusInfo[dbusInfoIndex]
                                 .propertyValues;
    auto it = std::find(propValues.begin(), propValues.end(), propertyValue);
    uint8_t newState{};
    if (it != propValues.end())
    {
        auto index = std::distance(propValues.begin(), it);
        newState = hostEffecterInfo[effecterInfoIndex]
                       .dbusInfo[dbusInfoIndex]
                       .state.states[index];
    }
    else
    {
        throw std::out_of_range("new state not found in json");
    }
    return newState;
}

void HostEffecterParser::setHostStateEffecter(
    size_t effecterInfoIndex, std::vector<set_effecter_state_field>& stateField,
    uint16_t effecterId)
{
    uint8_t& mctpEid = hostEffecterInfo[effecterInfoIndex].mctpEid;
    uint8_t& compEffCnt = hostEffecterInfo[effecterInfoIndex].compEffecterCnt;
    auto instanceId = requester.getInstanceId(mctpEid);

    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + sizeof(effecterId) + sizeof(compEffCnt) +
            sizeof(set_effecter_state_field) * compEffCnt,
        0);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_set_state_effecter_states_req(
        instanceId, effecterId, compEffCnt, stateField.data(), request);

    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message encode failure. PLDM error code = " << std::hex
                  << std::showbase << rc << "\n";
        requester.markFree(mctpEid, instanceId);
        return;
    }

    std::cout << "sending command: \n";
    if (requestMsg.size())
    {
        std::ostringstream tempStream;
        for (int byte : requestMsg)
        {
            tempStream << std::setfill('0') << std::setw(2) << std::hex << byte
                       << " ";
        }
        std::cout << tempStream.str() << std::endl;
    }

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};

    rc = pldm_send_recv(mctpEid, sockFd, requestMsg.data(), requestMsg.size(),
                        &responseMsg, &responseMsgSize);
    std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{responseMsg,
                                                                  std::free};
    requester.markFree(mctpEid, instanceId);

    if (rc != PLDM_REQUESTER_SUCCESS)
    {
        std::cerr << "Failed to send message/receive response. RC = " << rc
                  << ", errno = " << errno << "\n";
    }
    return;
}

} // namespace host_effecters
} // namespace pldm
