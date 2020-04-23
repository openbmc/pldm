#pragma once

#include "instance_id.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <utility> 

namespace pldm
{
namespace host_effecters
{

using Json = nlohmann::json;
using namespace pldm::utils;
using propName = std::string;
using DbusChgHostEffecterProps = std::map<propName, PropertyValue>;

struct State
{
    uint16_t stateSetId;
    std::vector<uint8_t> states;
};

struct DBusInfo
{
    std::string objectPath;
    std::string interface;
    std::string propertyName;
    std::string propertyType;
    std::vector<PropertyValue> propertyValues;
    State state;
};

struct EffecterInfo
{
    uint8_t mctpEid;
    uint16_t effecterId;
    uint16_t containerId;
    uint16_t entityType;
    uint16_t entityInstance;
    uint8_t compEffecterCnt;
    std::vector<DBusInfo> dbusInfo;
};

class HostEffecterParser
{
  public:
    HostEffecterParser() = delete;
    HostEffecterParser(const HostEffecterParser&) = delete;
    HostEffecterParser& operator=(const HostEffecterParser&) = delete;
    HostEffecterParser(HostEffecterParser&&) = delete;
    HostEffecterParser& operator=(HostEffecterParser&&) = delete;
    virtual ~HostEffecterParser() = default;

    HostEffecterParser(const pldm_pdr *repo): pdrRepo(repo)
    {
    }

    /* will be called from pldmd.cpp */
    void parseEffecterJson(const std::string& jsonPath);

    /* prototype yet to be completed */
   // fetchHostEffecterPDR();

    /* Handler for the match objects. prototype to be defined */
    void processHostEffecterChangeNotification(
        const DbusChgHostEffecterProps& chProperties,
        uint32_t effecterInfoIndex, uint32_t dbusInfoIndex);

    void populatePropVals(const Json& dBusValues,
                          std::vector<PropertyValue>& propertyValues,
                          const std::string& propertyType);

    void setHostStateEffecter(
        uint32_t effecterInfoIndex,
        std::vector<set_effecter_state_field>& stateField,
        uint8_t& instanceId);

    std::pair<uint8_t, uint8_t> findNewStateValue(uint32_t effecterInfoIndex,
                                uint32_t dbusInfoIndex,const PropertyValue& propertyValue);

    /* to obtain instance id before every pldm send */
    uint8_t getInstanceId(uint8_t eid);

    /* free the instance id after pldm_send_recv returns */
    void markFree(uint8_t instanceId);

  private:
    const pldm_pdr *pdrRepo;
     InstanceId id;
    /** @brief PLDM Instance ID vector */
    //std::vector<uint8_t> ids;
    std::vector<EffecterInfo> hostEffecterInfo;
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>>
        effecterInfoMatch;
};

} // namespace host_effecters
} // namespace pldm
