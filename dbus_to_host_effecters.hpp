#pragma once

#include "dbus_impl_requester.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

namespace pldm
{

using namespace utils;
using namespace dbus_api;

namespace host_effecters
{

using Json = nlohmann::json;
using PropName = std::string;
using StateValue = uint8_t;
using DbusChgHostEffecterProps = std::map<PropName, PropertyValue>;

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
    DBusMapping dbusMap;
    std::vector<PropertyValue> propertyValues; //!< D-Bus property values
    State state; //!< Corresponding effecter states
};

/** @struct EffecterInfo
 *  Contains the effecter information as a whole
 */
struct EffecterInfo
{
    uint8_t mctpEid;                //!< Host mctp eid
    uint16_t containerId;           //!< Container Id for host effecter
    uint16_t entityType;            //!< Entity type for the host effecter
    uint16_t entityInstance;        //!< Entity instance for the host effecter
    uint8_t compEffecterCnt;        //!< Composite effecter count
    std::vector<DBusInfo> dbusInfo; //!< D-Bus informations for the effecter id
};

/** @class HostEffecterParser
 *
 *  @brief This class parses the Host Effecter json file and monitors for the
 *         D-Bus changes for the effecters. Upon change, calls the corresponding
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

    /** @brief Constructor to create a HostEffecterParser object.
     *  @param[in] requester - PLDM Requester object pointer
     *  @param[in] fd - socket fd to communicate to host
     *  @param[in] repo -  PLDM pdr repository
     *  @param[in] dbusHandler - Dbus Handler
     */
    explicit HostEffecterParser(Requester* requester, int fd,
                                const pldm_pdr* repo,
                                DBusHandler* const dbusHandler) :
        requester(requester),
        sockFd(fd), pdrRepo(repo), dbusHandler(dbusHandler)
    {
    }

    /* @brief Parses the host effecter json
     *
     * @param[in] jsonPath - path for the json file
     */
    void parseEffecterJson(const std::string& jsonPath);

    /* @brief Method to take action when the subscribed D-Bus property is
     *        changed
     * @param[in] chProperties - list of properties which have changed
     * @param[in] effecterInfoIndex - index of effecterInfo pointer in
     *                                hostEffecterInfo
     * @param[in] dbusInfoIndex - index on dbusInfo pointer in each effecterInfo
     * @param[in] effecterId - host effecter id
     * @return - none
     */
    void processHostEffecterChangeNotification(
        const DbusChgHostEffecterProps& chProperties, size_t effecterInfoIndex,
        size_t dbusInfoIndex, uint16_t effecterId);

    /* @brief Populate the property values in each dbusInfo from the json
     *
     * @param[in] dBusValues - json values
     * @param[out] propertyValues - dbusInfo property values
     * @param[in] propertyType - type of the D-Bus property
     * @return - none
     */
    void populatePropVals(const Json& dBusValues,
                          std::vector<PropertyValue>& propertyValues,
                          const std::string& propertyType);

    /* @brief Set a host state effecter
     *
     * @param[in] effecterInfoIndex - index of effecterInfo pointer in
     *                                hostEffecterInfo
     * @param[in] stateField - vector of state fields equal to composite
     *                         effecter count in number
     * @param[in] effecterId - host effecter id
     */
    virtual void
        setHostStateEffecter(size_t effecterInfoIndex,
                             std::vector<set_effecter_state_field>& stateField,
                             uint16_t effecterId);

    /* @brief Fetches the new state value and the index in stateField set which
     *        needs to be set with the new value in the setStateEffecter call
     * @param[in] effecterInfoIndex - index of effecterInfo in hostEffecterInfo
     * @param[in] dbusInfoIndex - index of dbusInfo within effecterInfo
     * @param[in] propertyValue - the changed D-Bus property value
     * @return - the new state value
     */
    StateValue findNewStateValue(size_t effecterInfoIndex, size_t dbusInfoIndex,
                                 const PropertyValue& propertyValue);

    /* @brief Subscribes for DBus property change signal on the specified object
     *
     * @param[in] objectPath - DBus object path to look for
     * @param[in] interface - DBus interface
     * @param[in] effecterInfoIndex - index of effecterInfo pointer in
     *                                hostEffecterInfo
     * @param[in] dbusInfoIndex - index of dbusInfo within effecterInfo
     * @param[in] effecterId - host effecter id
     */
    virtual void createHostEffecterMatch(const std::string& objectPath,
                                         const std::string& interface,
                                         size_t effecterInfoIndex,
                                         size_t dbusInfoIndex,
                                         uint16_t effecterId);

  protected:
    Requester* requester;    //!< Reference to Requester to obtain instance id
    int sockFd;              //!< Socket fd to send message to host
    const pldm_pdr* pdrRepo; //!< Reference to PDR repo
    std::vector<EffecterInfo> hostEffecterInfo; //!< Parsed effecter information
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>>
        effecterInfoMatch; //!< vector to catch the D-Bus property change
                           //!< signals for the effecters
    DBusHandler* const dbusHandler; //!< Dbus Handler
};

} // namespace host_effecters
} // namespace pldm
