#pragma once

#include "dbus_impl_requester.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

namespace pldm
{
namespace host_effecters
{

using namespace pldm::utils;
using namespace pldm::dbus_api;

using Json = nlohmann::json;
using propName = std::string;
using stateValue = uint8_t;
using indexValue = uint8_t;
using DbusChgHostEffecterProps = std::map<propName, PropertyValue>;

/** @struct State
 *  Contains the state set id and the possible states for
 *  an effecter
 */
struct State
{
    uint16_t stateSetId;         //!< State set id
    std::vector<uint8_t> states; //!< Possible states
};

/** @struct DBusInfo
 *  Contains the D-Bus information for an effecter
 */
struct DBusInfo
{
    std::string objectPath;                    //!< D-Bus object path
    std::string interface;                     //!< D-Bus interface
    std::string propertyName;                  //!< D-Bus property name
    std::string propertyType;                  //!< D-Bus property type
    std::vector<PropertyValue> propertyValues; //!< D-Bus property values
    State state; //!< Corresponding effecter states
};

/** @struct EffecterInfo
 *  Contains the effecter information as a whole
 */
struct EffecterInfo
{
    uint8_t mctpEid;                //!< Host mctp eid
    uint16_t effecterId;            //!< Effecter id
    uint16_t containerId;           //!< Container Id for host
    uint16_t entityType;            //!< Entity type for the host
    uint16_t entityInstance;        //!< Entity instance for the host
    uint8_t compEffecterCnt;        //!< Composite effecter count
    std::vector<DBusInfo> dbusInfo; //!< D-Bus informations for the effecter id
};

/** @class HostEffecterParser
 *
 *  @brief Parses the Host Effecter json file and monitors for the D-Bus changes
 *         for the effecters. Upon change, calls the corresponding
 *         setStateEffecterStates on the host
 */
class HostEffecterParser
{
  public:
    HostEffecterParser() = delete;
    HostEffecterParser(const HostEffecterParser&) = delete;
    HostEffecterParser& operator=(const HostEffecterParser&) = delete;
    HostEffecterParser(HostEffecterParser&&) = delete;
    HostEffecterParser& operator=(HostEffecterParser&&) = delete;
    virtual ~HostEffecterParser() = default;

    explicit HostEffecterParser(Requester& requester, int fd,
                                const pldm_pdr* repo) :
        requester(requester),
        sockFd(fd), pdrRepo(repo)
    {
    }

    /* @brief Parses the host effecter json
     *
     * @param[in] jsonPath - path for the json file
     */
    void parseEffecterJson(const std::string& jsonPath);

    /* prototype yet to be completed */
    // fetchHostEffecterPDR();

    /* @brief Method to take action when the subscribed D-Bus property is
     * changed
     * @param[in] chProperties - list of properties which have changed
     * @param[in] effecterInfoIndex - index of effecterInfo pointer in
     * hostEffecterInfo
     * @param[in] dbusInfoIndex - index on dbusInfo pointer in each effecterInfo
     * @return - none
     */
    void processHostEffecterChangeNotification(
        const DbusChgHostEffecterProps& chProperties,
        uint32_t effecterInfoIndex, uint32_t dbusInfoIndex);

    /* @brief Populate the property values in each dbusInfo from the json
     * @param[in] dBusValues - json values
     * @param[in/out] propertyValues - dbusInfo property values
     * @param[in] propertyType - type of the D-Bus property
     * @return - none
     */
    void populatePropVals(const Json& dBusValues,
                          std::vector<PropertyValue>& propertyValues,
                          const std::string& propertyType);

    /* @brief Set a host state effecter
     * @param[in] effecterInfoIndex - index of effecterInfo pointer in
     * hostEffecterInfo
     * @param[in] stateField - vector of state fields equal to composite
     * effecter count in number
     * @return - none
     */
    void
        setHostStateEffecter(uint32_t effecterInfoIndex,
                             std::vector<set_effecter_state_field>& stateField);

    /* @brief Fetches the new state value and the index in stateField set which
     * needs to be set with the new value in the setStateEffecter call
     * @param[in] effecterInfoIndex - index of effecterInfo pointer in
     * hostEffecterInfo
     * @param[in] dbusInfoIndex - index on dbusInfo pointer in each effecterInfo
     * @param[in] propertyValue - the changed D-Bus property value
     * @return - pair of the new state value and index of the new state in the
     *  composite effecters
     */
    std::pair<stateValue, indexValue>
        findNewStateValue(uint32_t effecterInfoIndex, uint32_t dbusInfoIndex,
                          const PropertyValue& propertyValue);

  private:
    Requester& requester;    //!< Referrence to Requester to obtain instance id
    int sockFd;              //!< Socket fd to send message to host
    const pldm_pdr* pdrRepo; //!< Referrence to PDR repo
    std::vector<EffecterInfo> hostEffecterInfo; //!< Parsed effecter information
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>>
        effecterInfoMatch; //!< vector to catch the D-Bus property change
                           //!< signals for the effecters
};

} // namespace host_effecters
} // namespace pldm
