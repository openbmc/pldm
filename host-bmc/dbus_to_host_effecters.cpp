#include "dbus_to_host_effecters.hpp"

#include <libpldm/pdr.h>
#include <libpldm/platform.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/State/Boot/Progress/client.hpp>
#include <xyz/openbmc_project/State/OperatingSystem/Status/server.hpp>

#include <fstream>

PHOSPHOR_LOG2_USING;

using namespace pldm::utils;

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
    for (const auto& elem : dBusValues)
    {
        auto value = jsonEntryToDbusVal(propertyType, elem);
        propertyValues.emplace_back(value);
    }
}

void HostEffecterParser::parseEffecterJson(const std::string& jsonPath)
{
    fs::path jsonDir(jsonPath);
    if (!fs::exists(jsonDir) || fs::is_empty(jsonDir))
    {
        error("Effecter json file for remote terminus '{PATH}' does not exist.",
              "PATH", jsonPath);
        return;
    }

    fs::path jsonFilePath = jsonDir / hostEffecterJson;
    if (!fs::exists(jsonFilePath))
    {
        error("Json at path '{PATH}' does not exist.", "PATH", jsonFilePath);
        throw InternalFailure();
    }

    std::ifstream jsonFile(jsonFilePath);
    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        error("Failed to parse json file {PATH}", "PATH", jsonFilePath);
        throw InternalFailure();
    }
    const Json empty{};
    const std::vector<Json> emptyList{};

    auto entries = data.value("entries", emptyList);
    for (const auto& entry : entries)
    {
        EffecterInfo effecterInfo;
        effecterInfo.mctpEid = entry.value("mctp_eid", 0xFF);
        auto jsonEffecterInfo = entry.value("effecter_info", empty);
        auto effecterId = jsonEffecterInfo.value("effecterID",
                                                 PLDM_INVALID_EFFECTER_ID);
        effecterInfo.containerId = jsonEffecterInfo.value("containerID", 0);
        effecterInfo.entityType = jsonEffecterInfo.value("entityType", 0);
        effecterInfo.entityInstance = jsonEffecterInfo.value("entityInstance",
                                                             0);
        effecterInfo.compEffecterCnt =
            jsonEffecterInfo.value("compositeEffecterCount", 0);
        auto effecters = entry.value("effecters", emptyList);
        for (const auto& effecter : effecters)
        {
            DBusEffecterMapping dbusInfo{};
            auto jsonDbusInfo = effecter.value("dbus_info", empty);
            dbusInfo.dbusMap.objectPath = jsonDbusInfo.value("object_path", "");
            dbusInfo.dbusMap.interface = jsonDbusInfo.value("interface", "");
            dbusInfo.dbusMap.propertyName = jsonDbusInfo.value("property_name",
                                                               "");
            dbusInfo.dbusMap.propertyType = jsonDbusInfo.value("property_type",
                                                               "");
            Json propertyValues = jsonDbusInfo["property_values"];

            populatePropVals(propertyValues, dbusInfo.propertyValues,
                             dbusInfo.dbusMap.propertyType);

            const std::vector<uint8_t> emptyStates{};
            auto state = effecter.value("state", empty);
            dbusInfo.state.stateSetId = state.value("id", 0);
            auto states = state.value("state_values", emptyStates);
            if (dbusInfo.propertyValues.size() != states.size())
            {
                error(
                    "Number of states do not match with number of D-Bus property values in the json. Object path at '{PATH}' and property '{PROPERTY}' will not be monitored",
                    "PATH", dbusInfo.dbusMap.objectPath, "PROPERTY",
                    dbusInfo.dbusMap.propertyName);
                continue;
            }
            for (const auto& s : states)
            {
                dbusInfo.state.states.emplace_back(s);
            }

            auto effecterInfoIndex = hostEffecterInfo.size();
            auto dbusInfoIndex = effecterInfo.dbusInfo.size();
            createHostEffecterMatch(
                dbusInfo.dbusMap.objectPath, dbusInfo.dbusMap.interface,
                effecterInfoIndex, dbusInfoIndex, effecterId);
            effecterInfo.dbusInfo.emplace_back(std::move(dbusInfo));
        }
        hostEffecterInfo.emplace_back(std::move(effecterInfo));
    }
}

void HostEffecterParser::processHostEffecterChangeNotification(
    const DbusChgHostEffecterProps& chProperties, size_t effecterInfoIndex,
    size_t dbusInfoIndex, uint16_t effecterId)
{
    using BootProgress =
        sdbusplus::client::xyz::openbmc_project::state::boot::Progress<>;

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
        constexpr auto localOrRemote = false;
        effecterId = findStateEffecterId(
            pdrRepo, hostEffecterInfo[effecterInfoIndex].entityType,
            hostEffecterInfo[effecterInfoIndex].entityInstance,
            hostEffecterInfo[effecterInfoIndex].containerId,
            hostEffecterInfo[effecterInfoIndex]
                .dbusInfo[dbusInfoIndex]
                .state.stateSetId,
            localOrRemote);
        if (effecterId == PLDM_INVALID_EFFECTER_ID)
        {
            error(
                "Effecter ID '{EFFECTERID}' of entity type '{TYPE}', entityInstance '{INSTANCE}' and containerID '{CONTAINER_ID}' not found in pdr repo",
                "EFFECTERID", effecterId, "TYPE",
                hostEffecterInfo[effecterInfoIndex].entityType, "INSTANCE",
                hostEffecterInfo[effecterInfoIndex].entityInstance,
                "CONTAINER_ID",
                hostEffecterInfo[effecterInfoIndex].containerId);
            return;
        }
    }
    constexpr auto hostStatePath = "/xyz/openbmc_project/state/host0";

    try
    {
        auto propVal = dbusHandler->getDbusPropertyVariant(
            hostStatePath, "BootProgress", BootProgress::interface);

        using Stages = BootProgress::ProgressStages;
        auto currHostState = sdbusplus::message::convert_from_string<Stages>(
                                 std::get<std::string>(propVal))
                                 .value();

        if (currHostState != Stages::SystemInitComplete &&
            currHostState != Stages::OSRunning &&
            currHostState != Stages::SystemSetup)
        {
            info(
                "Remote terminus is not up/active, current remote terminus state is: '{CURRENT_HOST_STATE}'",
                "CURRENT_HOST_STATE", currHostState);
            return;
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Error in getting current remote terminus state. Will still continue to set the remote terminus effecter, error - {ERROR}",
            "ERROR", e);
    }
    uint8_t newState{};
    try
    {
        newState = findNewStateValue(effecterInfoIndex, dbusInfoIndex,
                                     it->second);
    }
    catch (const std::out_of_range& e)
    {
        error("Failed to find new state '{NEW_STATE}' in json, error - {ERROR}",
              "ERROR", e, "NEW_STATE", newState);
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
    int rc{};
    try
    {
        rc = setHostStateEffecter(effecterInfoIndex, stateField, effecterId);
    }
    catch (const std::runtime_error& e)
    {
        error(
            "Failed to set remote terminus state effecter for effecter ID '{EFFECTERID}', error - {ERROR}",
            "ERROR", e, "EFFECTERID", effecterId);
        return;
    }
    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to set the remote terminus state effecter for effecter ID '{EFFECTERID}', response code '{RC}'",
            "EFFECTERID", effecterId, "RC", rc);
    }
}

uint8_t
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

int HostEffecterParser::setHostStateEffecter(
    size_t effecterInfoIndex, std::vector<set_effecter_state_field>& stateField,
    uint16_t effecterId)
{
    uint8_t& mctpEid = hostEffecterInfo[effecterInfoIndex].mctpEid;
    uint8_t& compEffCnt = hostEffecterInfo[effecterInfoIndex].compEffecterCnt;
    auto instanceId = instanceIdDb->next(mctpEid);

    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + sizeof(effecterId) + sizeof(compEffCnt) +
            sizeof(set_effecter_state_field) * compEffCnt,
        0);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_set_state_effecter_states_req(
        instanceId, effecterId, compEffCnt, stateField.data(), request);

    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to encode set state effecter states message for effecter ID '{EFFECTERID}' and instanceID '{INSTANCE}' with response code '{RC}'",
            "EFFECTERID", effecterId, "INSTANCE", instanceId, "RC", lg2::hex,
            rc);
        instanceIdDb->free(mctpEid, instanceId);
        return rc;
    }

    auto setStateEffecterStatesRespHandler =
        [](mctp_eid_t /*eid*/, const pldm_msg* response, size_t respMsgLen) {
        if (response == nullptr || !respMsgLen)
        {
            error(
                "Failed to receive response for setting state effecter states.");
            return;
        }
        uint8_t completionCode{};
        auto rc = decode_set_state_effecter_states_resp(response, respMsgLen,
                                                        &completionCode);
        if (rc)
        {
            error(
                "Failed to decode response of set state effecter states, response code '{RC}'",
                "RC", rc);
            pldm::utils::reportError(
                "xyz.openbmc_project.bmc.pldm.SetHostEffecterFailed");
        }
        if (completionCode)
        {
            error(
                "Failed to set a remote terminus effecter, completion code '{CC}'",
                "CC", completionCode);
            pldm::utils::reportError(
                "xyz.openbmc_project.bmc.pldm.SetHostEffecterFailed");
        }
    };

    rc = handler->registerRequest(
        mctpEid, instanceId, PLDM_PLATFORM, PLDM_SET_STATE_EFFECTER_STATES,
        std::move(requestMsg), std::move(setStateEffecterStatesRespHandler));
    if (rc)
    {
        error(
            "Failed to send request to set an effecter on remote terminus for effecter ID '{EFFECTERID}', response code '{RC}'",
            "EFFECTERID", effecterId, "RC", rc);
    }
    return rc;
}

void HostEffecterParser::createHostEffecterMatch(const std::string& objectPath,
                                                 const std::string& interface,
                                                 size_t effecterInfoIndex,
                                                 size_t dbusInfoIndex,
                                                 uint16_t effecterId)
{
    using namespace sdbusplus::bus::match::rules;
    effecterInfoMatch.emplace_back(std::make_unique<sdbusplus::bus::match_t>(
        pldm::utils::DBusHandler::getBus(),
        propertiesChanged(objectPath, interface),
        [this, effecterInfoIndex, dbusInfoIndex,
         effecterId](sdbusplus::message_t& msg) {
        DbusChgHostEffecterProps props;
        std::string iface;
        msg.read(iface, props);
        processHostEffecterChangeNotification(props, effecterInfoIndex,
                                              dbusInfoIndex, effecterId);
    }));
}

} // namespace host_effecters
} // namespace pldm
