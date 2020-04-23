#include "dbus_to_host_effecters.hpp"

#include "utils.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/State/OperatingSystem/Status/server.hpp>

#include "libpldm/requester/pldm.h"

namespace pldm
{
namespace host_effecters
{

using Json = nlohmann::json;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr auto hostEffecterJson = "dbus_to_host_effecter.json";

void HostEffecterParser::populatePropVals(
    const Json& dBusValues, std::vector<PropertyValue>& propertyValues,
    const std::string& propertyType)

{
    PropertyValue value;
    for (auto it = dBusValues.begin(); it != dBusValues.end(); ++it)
    {
        if (propertyType == "uint8_t")
        {
            value = static_cast<uint8_t>(it.value());
        }
        else if (propertyType == "uint16_t")
        {
            value = static_cast<uint16_t>(it.value());
        }
        else if (propertyType == "uint32_t")
        {
            value = static_cast<uint32_t>(it.value());
        }
        else if (propertyType == "uint64_t")
        {
            value = static_cast<uint64_t>(it.value());
        }
        else if (propertyType == "int16_t")
        {
            value = static_cast<int16_t>(it.value());
        }
        else if (propertyType == "int32_t")
        {
            value = static_cast<int32_t>(it.value());
        }
        else if (propertyType == "int64_t")
        {
            value = static_cast<int64_t>(it.value());
        }
        else if (propertyType == "bool")
        {
            value = static_cast<bool>(it.value());
        }
        else if (propertyType == "double")
        {
            value = static_cast<double>(it.value());
        }
        else if (propertyType == "string")
        {
            value = static_cast<std::string>(it.value());
        }
        else
        {
            std::cerr << "Unknown D-Bus property type, TYPE="
                      << propertyType.c_str() << "\n";
            return;
        }
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
        effecterInfo.effecterId = entry.value("effecterID", 0);
        auto jsonEffecterInfo = entry.value("effecter_info", empty);
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
            dbusInfo.objectPath = jsonDbusInfo.value("object_path", "");
            dbusInfo.interface = jsonDbusInfo.value("interface", "");
            dbusInfo.propertyName = jsonDbusInfo.value("property_name", "");
            dbusInfo.propertyType = jsonDbusInfo.value("property_type", "");
            Json propertyValues = jsonDbusInfo["property_values"];

            populatePropVals(propertyValues, dbusInfo.propertyValues,
                             dbusInfo.propertyType);

            const std::vector<uint8_t> emptyStates{};
            auto state = effecter.value("state", empty);
            dbusInfo.state.stateSetId = state.value("id", 0);
            auto states = state.value("states", emptyStates);
            for (const auto& s : states)
            {
                dbusInfo.state.states.push_back(s);
            }

            auto effecterInfoIndex = hostEffecterInfo.size();
            auto dbusInfoIndex = effecterInfo.dbusInfo.size();
            using namespace sdbusplus::bus::match::rules;
            static auto bus = sdbusplus::bus::new_default();
            effecterInfoMatch.push_back(
                std::make_unique<sdbusplus::bus::match::match>(
                    /*pldm::utils::DBusHandler::getBus(),*/ bus,
                    propertiesChanged(dbusInfo.objectPath, dbusInfo.interface),
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
    const DbusChgHostEffecterProps& chProperties, uint32_t effecterInfoIndex,
    uint32_t dbusInfoIndex)
{
    const auto& propertyName = hostEffecterInfo[effecterInfoIndex]
                                   .dbusInfo[dbusInfoIndex]
                                   .propertyName;
    const auto& it = chProperties.find(propertyName);

    if (it == chProperties.end())
    {
        return;
    }

    if (hostEffecterInfo[effecterInfoIndex].effecterId == 0xFF)
    {
        // fetch pdr
    }
    else if (hostEffecterInfo[effecterInfoIndex].effecterId ==
             0x4) // hardcoded effecter id for Phyp
    {
        const auto& newStringValue = std::get<std::string>(it->second);
        if (newStringValue !=
            "xyz.openbmc_project.Control.Boot.Mode.Modes.Regular")
        {
            return;
        }
        /* constexpr auto hostStateInterface =
           "xyz.openbmc_project.State.OperatingSystem.Status";
       constexpr auto hostStatePath = "/xyz/openbmc_project/state/host0";

       try
       {
           using namespace
       sdbusplus::xyz::openbmc_project::State::OperatingSystem::server; auto
       currHostState = pldm::utils::DBusHandler().getDbusProperty<std::string>(
                   hostStatePath, "OperatingSystemState", hostStateInterface);
           if (currHostState !=
       "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.Standby")
               {
                   std::cout <<"Host is not at standby. Current host state: " <<
       currHostState.c_str() << "\n"; return;
               }
       }
       catch (const sdbusplus::exception::SdBusError& e)
       {
           std::cerr << "Error in getting current host state. Will still "
                        "continue to set the host effecter \n";
       }*/
    }
    auto [newState, index] =
        findNewStateValue(effecterInfoIndex, dbusInfoIndex, it->second);

    if (index == 0xF)
    {
        std::cerr << "The changed d-bus value is not included in the json. "
                     "Host Effecter could not be set \n";
        return;
    }

    std::vector<set_effecter_state_field> stateField;
    for (auto i = 0; i < hostEffecterInfo[effecterInfoIndex].compEffecterCnt;
         i++) // fill up the state field
    {
        if (i == index)
        {
            stateField.push_back({PLDM_REQUEST_SET, newState});
        }
        else
        {
            stateField.push_back({PLDM_NO_CHANGE, 0});
        }
    }
    uint8_t instanceId{};
    try
    {
        setHostStateEffecter(effecterInfoIndex, stateField, instanceId);
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Could not set host state effecter \n";
        return;
    }
    id.markFree(instanceId);
}

std::pair<uint8_t, uint8_t>
    HostEffecterParser::findNewStateValue(uint32_t effecterInfoIndex,
                                          uint32_t dbusInfoIndex,
                                          const PropertyValue& propertyValue)
{
    const auto& propValues = hostEffecterInfo[effecterInfoIndex]
                                 .dbusInfo[dbusInfoIndex]
                                 .propertyValues;
    auto it = std::find(propValues.begin(), propValues.end(), propertyValue);
    uint8_t index = 0xF;
    uint8_t newState{};
    if (it != propValues.end())
    {
        index = std::distance(propValues.begin(), it);
        newState = hostEffecterInfo[effecterInfoIndex]
                       .dbusInfo[dbusInfoIndex]
                       .state.states[index];
    }
    return {newState, index};
}

void HostEffecterParser::setHostStateEffecter(
    uint32_t effecterInfoIndex,
    std::vector<set_effecter_state_field>& stateField, uint8_t& instanceId)
{
    instanceId = id.next();

    auto effId = hostEffecterInfo[effecterInfoIndex].effecterId;
    auto compEffCnt = hostEffecterInfo[effecterInfoIndex].compEffecterCnt;

    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + sizeof(effId) + sizeof(compEffCnt) +
            sizeof(set_effecter_state_field) * compEffCnt,
        0);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_set_state_effecter_states_req(
        instanceId, effId, compEffCnt, stateField.data(), request);

    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message encode failure. PLDM error code = " << std::hex
                  << std::showbase << rc << "\n";
        return;
    }
    int fd = pldm_open();
    if (-1 == fd)
    {
        std::cerr << "Failed to init mctp \n";
        return;
    }
    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};

    rc = pldm_send_recv(hostEffecterInfo[effecterInfoIndex].mctpEid, fd,
                        requestMsg.data(), requestMsg.size(), &responseMsg,
                        &responseMsgSize);

    if (0 > rc)
    {
        std::cerr << "Failed to send message/receive response. RC = " << rc
                  << ", errno = " << errno << "\n";
        return;
    }
    free(responseMsg);
    return;
}

} // namespace host_effecters
} // namespace pldm
