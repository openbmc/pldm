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
        effecterInfo.effecterId = jsonEffecterInfo.value("effecterID", 0);
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
                    [this, effecterInfoIndex,
                     dbusInfoIndex](sdbusplus::message::message& msg) {
                        DbusChgHostEffecterProps props;
                        std::string iface;
                        msg.read(iface, props);
                        processHostEffecterChangeNotification(
                            props, effecterInfoIndex, dbusInfoIndex);
                    }));

            effecterInfo.dbusInfo.push_back(std::move(dbusInfo));
        }
        hostEffecterInfo.push_back(std::move(effecterInfo));
    }
}

void HostEffecterParser::processHostEffecterChangeNotification(
    const DbusChgHostEffecterProps& chProperties, size_t effecterInfoIndex,
    size_t dbusInfoIndex)
{
    const auto& propertyName = hostEffecterInfo[effecterInfoIndex]
                                   .dbusInfo[dbusInfoIndex]
                                   .dbusMap.propertyName;

    const auto& it = chProperties.find(propertyName);

    if (it == chProperties.end())
    {
        return;
    }

    if (hostEffecterInfo[effecterInfoIndex].effecterId == 0xFFFF)
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
                hostEffecterInfo[effecterInfoIndex].effecterId =
                    pdr->effecter_id;
                break;
            }
            pdrData = nullptr;
            record = pldm_pdr_find_record_by_type(
                pdrRepo, PLDM_STATE_EFFECTER_PDR, record, &pdrData, &pdrSize);
        }
    }
    else if (hostEffecterInfo[effecterInfoIndex].effecterId ==
             0x4) // hardcoded effecter id for Phyp
    {
        const auto& bootMode = std::get<std::string>(it->second);

        if (bootMode != "xyz.openbmc_project.Control.Boot.Mode.Modes.Regular")
        {
            return;
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
            if (currHostState != "xyz.openbmc_project.State.OperatingSystem."
                                 "Status.OSStatus.Standby")
            {
                std::cout << "Host is not at standby. Current host state: "
                          << currHostState.c_str() << "\n";
                return;
            }
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {
            std::cerr << "Error in getting current host state. Will still "
                         "continue to set the host effecter \n";
        }
    }
    auto [newState, index] =
        findNewStateValue(effecterInfoIndex, dbusInfoIndex, it->second);

    if (index == 0xFF)
    {
        std::cerr << "The changed d-bus value is not included in the json. "
                     "Host Effecter could not be set \n";
        return;
    }

    std::vector<set_effecter_state_field> stateField;
    for (size_t i = 0; i < hostEffecterInfo[effecterInfoIndex].compEffecterCnt;
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
        setHostStateEffecter(effecterInfoIndex, stateField);
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Could not set host state effecter \n";
        if (hostEffecterInfo[effecterInfoIndex].effecterId != 0x4)
        {
            hostEffecterInfo[effecterInfoIndex].effecterId =
                PLDM_INVALID_EFFECTER_ID;
        }
        return;
    }
    if (hostEffecterInfo[effecterInfoIndex].effecterId !=
        0x4) // storing the effecter id only for Phyp
    {
        hostEffecterInfo[effecterInfoIndex].effecterId =
            PLDM_INVALID_EFFECTER_ID;
    }
}

std::pair<StateValue, IndexValue>
    HostEffecterParser::findNewStateValue(size_t effecterInfoIndex,
                                          size_t dbusInfoIndex,
                                          const PropertyValue& propertyValue)
{
    const auto& propValues = hostEffecterInfo[effecterInfoIndex]
                                 .dbusInfo[dbusInfoIndex]
                                 .propertyValues;
    auto it = std::find(propValues.begin(), propValues.end(), propertyValue);
    uint8_t index = 0xFF;
    uint8_t newState{};
    if (it != propValues.end())
    {
        index = std::distance(propValues.begin(), it);
        newState = hostEffecterInfo[effecterInfoIndex]
                       .dbusInfo[dbusInfoIndex]
                       .state.states[index];
    }
    return {newState, (index - 1)};
}

void HostEffecterParser::setHostStateEffecter(
    size_t effecterInfoIndex, std::vector<set_effecter_state_field>& stateField)
{
    auto mctpEid = hostEffecterInfo[effecterInfoIndex].mctpEid;
    auto instanceId = requester.getInstanceId(mctpEid);

    auto effecterId = hostEffecterInfo[effecterInfoIndex].effecterId;
    auto compEffCnt = hostEffecterInfo[effecterInfoIndex].compEffecterCnt;

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

    std::unique_ptr<uint8_t*> responseMsg{};
    size_t responseMsgSize{};

    rc = pldm_send_recv(mctpEid, sockFd, requestMsg.data(), requestMsg.size(),
                        responseMsg.get(), &responseMsgSize);
    requester.markFree(mctpEid, instanceId);

    responseMsg.release();

    if (rc != PLDM_REQUESTER_SUCCESS)
    {
        std::cerr << "Failed to send message/receive response. RC = " << rc
                  << ", errno = " << errno << "\n";
    }
    return;
}

} // namespace host_effecters
} // namespace pldm
