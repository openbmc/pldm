#include "dbus_to_host_effecters.hpp"

#include "libpldm/pdr.h"
#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/State/OperatingSystem/Status/server.hpp>

#include <fstream>
#include <iostream>

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

double HostEffecterParser::getEffecterValue(std::string objectPath,
                                            std::string interface,
                                            std::string propertyName)
{
    try
    {
        auto service =
            dbusHandler->getService(objectPath.c_str(), interface.c_str());
        if (!service.empty())
        {
            auto propertyValue = dbusHandler->getDbusPropertyVariant(
                objectPath.c_str(), propertyName.c_str(), interface.c_str());
            return std::get<double>(propertyValue);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to get effecter value objectPath=" << objectPath
                  << " interface=" << interface << " property=" << propertyName
                  << std::endl;
    }

    return std::numeric_limits<double>::quiet_NaN();
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
        std::cerr << "json does not exist, PATH=" << jsonFilePath << "\n";
        throw InternalFailure();
    }

    std::ifstream jsonFile(jsonFilePath);
    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        std::cerr << "Parsing json file failed, FILE=" << jsonFilePath << "\n";
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
        /* default as PLDM_STATE_EFFECTER_PDR */
        auto effecterPdrType =
            jsonEffecterInfo.value("effecterPdrType", PLDM_STATE_EFFECTER_PDR);
        auto effecterId =
            jsonEffecterInfo.value("effecterID", PLDM_INVALID_EFFECTER_ID);
        if (effecterPdrType != PLDM_STATE_EFFECTER_PDR &&
            effecterPdrType != PLDM_NUMERIC_EFFECTER_PDR)
        {
            std::cerr << "Invalid EffecterPDRType " << effecterPdrType
                      << " of effecterID " << effecterId << std::endl;
            continue;
        }
        effecterInfo.effecterPdrType = effecterPdrType;
        effecterInfo.containerId = jsonEffecterInfo.value("containerID", 0);
        effecterInfo.entityType = jsonEffecterInfo.value("entityType", 0);
        effecterInfo.entityInstance =
            jsonEffecterInfo.value("entityInstance", 0);
        effecterInfo.compEffecterCnt =
            jsonEffecterInfo.value("compositeEffecterCount", 0);
        effecterInfo.checkHostState =
            jsonEffecterInfo.value("checkHostState", true);
        auto effecters = entry.value("effecters", emptyList);
        if (effecterPdrType == PLDM_STATE_EFFECTER_PDR)
        {
            for (const auto& effecter : effecters)
            {
                DBusEffecterMapping dbusInfo{};
                auto jsonDbusInfo = effecter.value("dbus_info", empty);
                dbusInfo.dbusMap.objectPath =
                    jsonDbusInfo.value("object_path", "");
                dbusInfo.dbusMap.interface =
                    jsonDbusInfo.value("interface", "");
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
                auto states = state.value("state_values", emptyStates);
                if (dbusInfo.propertyValues.size() != states.size())
                {
                    std::cerr
                        << "Number of states do not match with"
                        << " number of D-Bus property values in the json. "
                        << "Object path " << dbusInfo.dbusMap.objectPath
                        << " and property " << dbusInfo.dbusMap.propertyName
                        << " will not be monitored \n";
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
        }
        else if (effecterPdrType == PLDM_NUMERIC_EFFECTER_PDR)
        {
            for (const auto& effecter : effecters)
            {
                DBusNumericEffecterMapping dbusInfo{};
                auto jsonDbusInfo = effecter.value("dbus_info", empty);
                dbusInfo.dataSize = effecter.value("effecterDataSize", 0);
                dbusInfo.unitModifier = effecter.value("unitModifier", 0);
                dbusInfo.resolution = effecter.value("resolution", 1);
                dbusInfo.offset = effecter.value("offset", 0);
                dbusInfo.dbusMap.objectPath =
                    jsonDbusInfo.value("object_path", "");
                dbusInfo.dbusMap.interface =
                    jsonDbusInfo.value("interface", "");
                dbusInfo.dbusMap.propertyName =
                    jsonDbusInfo.value("property_name", "");
                dbusInfo.dbusMap.propertyType =
                    jsonDbusInfo.value("property_type", "");

                dbusInfo.propertyValue = getEffecterValue(
                    dbusInfo.dbusMap.objectPath, dbusInfo.dbusMap.interface,
                    dbusInfo.dbusMap.propertyName);
                auto effecterInfoIndex = hostEffecterInfo.size();
                auto dbusInfoIndex = effecterInfo.dbusInfo.size();
                createHostEffecterMatch(
                    dbusInfo.dbusMap.objectPath, dbusInfo.dbusMap.interface,
                    effecterInfoIndex, dbusInfoIndex, effecterId);
                effecterInfo.dbusNumericEffecterInfo.emplace_back(
                    std::move(dbusInfo));
            }
        }
        hostEffecterInfo.emplace_back(std::move(effecterInfo));
    }
}

bool HostEffecterParser::isHostOn(void)
{
    constexpr auto hostStateInterface =
        "xyz.openbmc_project.State.Boot.Progress";
    constexpr auto hostStatePath = "/xyz/openbmc_project/state/host0";
    try
    {
        auto propVal = dbusHandler->getDbusPropertyVariant(
            hostStatePath, "BootProgress", hostStateInterface);
        const auto& currHostState = std::get<std::string>(propVal);
        if ((currHostState != "xyz.openbmc_project.State.Boot.Progress."
                              "ProgressStages.SystemInitComplete") &&
            (currHostState != "xyz.openbmc_project.State.Boot.Progress."
                              "ProgressStages.OSRunning") &&
            (currHostState != "xyz.openbmc_project.State.Boot.Progress."
                              "ProgressStages.OSStart"))
        {
            std::cout << "Host is not up. Current host state: "
                      << currHostState.c_str() << "\n";
            return true;
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        std::cerr << "Error in getting current host state. Will still "
                     "continue to set the host effecter \n";
    }
    return false;
}

void HostEffecterParser::processHostEffecterChangeNotification(
    const DbusChgHostEffecterProps& chProperties, size_t effecterInfoIndex,
    size_t dbusInfoIndex, uint16_t effecterId)
{
    const auto& pdrType = hostEffecterInfo[effecterInfoIndex].effecterPdrType;
    if (pdrType == PLDM_NUMERIC_EFFECTER_PDR)
    {
        processTerminusNumericEffecterChangeNotification(
            chProperties, effecterInfoIndex, dbusInfoIndex, effecterId);
        return;
    }
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
            std::cerr << "Effecter id not found in pdr repo \n";
            return;
        }
    }

    if (!isHostOn())
    {
        return;
    }

    uint8_t newState{};
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
    int rc{};
    try
    {
        rc = setHostStateEffecter(effecterInfoIndex, stateField, effecterId);
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Could not set host state effecter \n";
        return;
    }
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Could not set the host state effecter, rc= " << rc
                  << " \n";
    }
}

double HostEffecterParser::adjustValue(double value, double offset,
                                       double resolution, int8_t modify)
{
    double unitModifier = std::pow(10, signed(modify));
    return std::round((value - offset) * resolution / unitModifier);
}

void HostEffecterParser::processTerminusNumericEffecterChangeNotification(
    const DbusChgHostEffecterProps& chProperties, size_t effecterInfoIndex,
    size_t dbusInfoIndex, uint16_t effecterId)
{
    const auto& checkHost = hostEffecterInfo[effecterInfoIndex].checkHostState;
    const auto& propValues = hostEffecterInfo[effecterInfoIndex]
                                 .dbusNumericEffecterInfo[dbusInfoIndex];
    const auto& propertyName = propValues.dbusMap.propertyName;

    const auto& it = chProperties.find(propertyName);

    if (it == chProperties.end())
    {
        return;
    }

    double val = std::get<double>(it->second);
    /* the value of numeric effecter in D-Bus interface is double */
    if (std::isnan(propValues.propertyValue) || std::isnan(val))
    {
        std::cerr << " Bypass the first update of "
                  << propValues.dbusMap.objectPath << std::endl;
        hostEffecterInfo[effecterInfoIndex]
            .dbusNumericEffecterInfo[dbusInfoIndex]
            .propertyValue = val;
        return;
    }

    if (val == propValues.propertyValue)
    {
        std::cerr << " The configuring value is using for"
                  << propValues.dbusMap.objectPath << std::endl;
        return;
    }
    double rawValue = adjustValue(val, propValues.offset, propValues.resolution,
                                  propValues.unitModifier);

    if (checkHost && !isHostOn())
    {
        return;
    }

    int rc{};
    try
    {
        rc = setTerminusNumericEffecter(effecterInfoIndex, effecterId,
                                        propValues.dataSize, rawValue);
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Could not set numeric effecter ID=" << effecterId
                  << std::endl;
        return;
    }
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Could not set the numeric effecter, ID=" << effecterId
                  << " rc= " << rc << std::endl;
    }

    return;
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

int HostEffecterParser::setTerminusNumericEffecter(size_t effecterInfoIndex,
                                                   uint16_t effecterId,
                                                   uint8_t dataSize,
                                                   double rawValue)
{
    uint8_t& mctpEid = hostEffecterInfo[effecterInfoIndex].mctpEid;
    auto instanceId = requester->getInstanceId(mctpEid);
    int rc = PLDM_ERROR;
    std::vector<uint8_t> requestMsg;

    if ((dataSize == PLDM_EFFECTER_DATA_SIZE_UINT8) ||
        (dataSize == PLDM_EFFECTER_DATA_SIZE_SINT8))
    {
        requestMsg.resize(sizeof(pldm_msg_hdr) +
                          PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES);
        uint8_t effecter_value = (uint8_t)rawValue;
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        rc = encode_set_numeric_effecter_value_req(
            instanceId, effecterId, dataSize,
            reinterpret_cast<uint8_t*>(&effecter_value), request,
            PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES);
    }
    else if (dataSize == PLDM_EFFECTER_DATA_SIZE_UINT16)
    {
        requestMsg.resize(sizeof(pldm_msg_hdr) +
                          PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 1);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        uint16_t effecter_value = (uint16_t)rawValue;
        rc = encode_set_numeric_effecter_value_req(
            instanceId, effecterId, dataSize,
            reinterpret_cast<uint8_t*>(&effecter_value), request,
            PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 1);
    }
    else if (dataSize == PLDM_EFFECTER_DATA_SIZE_SINT16)
    {
        requestMsg.resize(sizeof(pldm_msg_hdr) +
                          PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 1);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        int16_t effecter_value = (int16_t)rawValue;
        rc = encode_set_numeric_effecter_value_req(
            instanceId, effecterId, dataSize,
            reinterpret_cast<uint8_t*>(&effecter_value), request,
            PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 1);
    }
    else if (dataSize == PLDM_EFFECTER_DATA_SIZE_UINT32)
    {
        requestMsg.resize(sizeof(pldm_msg_hdr) +
                          PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 3);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        uint32_t effecter_value = (uint32_t)rawValue;
        rc = encode_set_numeric_effecter_value_req(
            instanceId, effecterId, dataSize,
            reinterpret_cast<uint8_t*>(&effecter_value), request,
            PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 3);
    }
    else if (dataSize == PLDM_EFFECTER_DATA_SIZE_SINT32)
    {
        requestMsg.resize(sizeof(pldm_msg_hdr) +
                          PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 3);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        int32_t effecter_value = (int32_t)rawValue;
        rc = encode_set_numeric_effecter_value_req(
            instanceId, effecterId, dataSize,
            reinterpret_cast<uint8_t*>(&effecter_value), request,
            PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 3);
    }

    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message encode failure. PLDM error code = " << std::hex
                  << std::showbase << rc << "\n";
        requester->markFree(mctpEid, instanceId);
        return rc;
    }

    auto setNumericEffecterRespHandler = [](mctp_eid_t /*eid*/,
                                            const pldm_msg* response,
                                            size_t respMsgLen) {
        if (response == nullptr || !respMsgLen)
        {
            std::cerr << "Failed to receive response for "
                      << "setNumericEffecterValue command \n";
            return;
        }
        uint8_t completionCode{};
        auto rc = decode_set_numeric_effecter_value_resp(response, respMsgLen,
                                                         &completionCode);

        if (rc)
        {
            std::cerr << "Failed to decode setNumericEffecterValue response,"
                      << " rc " << rc << "\n";
        }
        if (completionCode)
        {
            std::cerr << "Failed to set a Host effecter "
                      << ", cc=" << static_cast<unsigned>(completionCode)
                      << "\n";
        }
    };

    rc = handler->registerRequest(
        mctpEid, instanceId, PLDM_PLATFORM, PLDM_SET_NUMERIC_EFFECTER_VALUE,
        std::move(requestMsg), std::move(setNumericEffecterRespHandler));
    if (rc)
    {
        std::cerr << "Failed to send request to set an effecter on Host \n";
    }
    return rc;
}

int HostEffecterParser::setHostStateEffecter(
    size_t effecterInfoIndex, std::vector<set_effecter_state_field>& stateField,
    uint16_t effecterId)
{
    uint8_t& mctpEid = hostEffecterInfo[effecterInfoIndex].mctpEid;
    uint8_t& compEffCnt = hostEffecterInfo[effecterInfoIndex].compEffecterCnt;
    auto instanceId = requester->getInstanceId(mctpEid);

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
        requester->markFree(mctpEid, instanceId);
        return rc;
    }

    auto setStateEffecterStatesRespHandler =
        [](mctp_eid_t /*eid*/, const pldm_msg* response, size_t respMsgLen) {
            if (response == nullptr || !respMsgLen)
            {
                std::cerr << "Failed to receive response for "
                          << "setStateEffecterStates command \n";
                return;
            }
            uint8_t completionCode{};
            auto rc = decode_set_state_effecter_states_resp(
                response, respMsgLen, &completionCode);
            if (rc)
            {
                std::cerr << "Failed to decode setStateEffecterStates response,"
                          << " rc " << rc << "\n";
                pldm::utils::reportError(
                    "xyz.openbmc_project.bmc.pldm.SetHostEffecterFailed");
            }
            if (completionCode)
            {
                std::cerr << "Failed to set a Host effecter "
                          << ", cc=" << static_cast<unsigned>(completionCode)
                          << "\n";
                pldm::utils::reportError(
                    "xyz.openbmc_project.bmc.pldm.SetHostEffecterFailed");
            }
        };

    rc = handler->registerRequest(
        mctpEid, instanceId, PLDM_PLATFORM, PLDM_SET_STATE_EFFECTER_STATES,
        std::move(requestMsg), std::move(setStateEffecterStatesRespHandler));
    if (rc)
    {
        std::cerr << "Failed to send request to set an effecter on Host \n";
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
    effecterInfoMatch.emplace_back(
        std::make_unique<sdbusplus::bus::match::match>(
            pldm::utils::DBusHandler::getBus(),
            propertiesChanged(objectPath, interface),
            [this, effecterInfoIndex, dbusInfoIndex,
             effecterId](sdbusplus::message::message& msg) {
                DbusChgHostEffecterProps props;
                std::string iface;
                msg.read(iface, props);
                processHostEffecterChangeNotification(
                    props, effecterInfoIndex, dbusInfoIndex, effecterId);
            }));
}

} // namespace host_effecters
} // namespace pldm
