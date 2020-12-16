#pragma once

#include "common/utils.hpp"
#include "libpldmresponder/platform.hpp"

#include <string>

using namespace pldm::utils;
namespace pldm
{
namespace responder
{

static constexpr uint8_t pSideNum = 1;
static constexpr uint8_t tSideNum = 2;
static constexpr auto Pside = "P";
static constexpr auto Tside = "T";

static constexpr auto redundancyIntf =
    "xyz.openbmc_project.Software.RedundancyPriority";

/** @class CodeUpdate
 *
 *  @brief This class performs the necessary operation in pldm for
 *         inband code update. That includes taking actions on the
 *         setStateEffecterStates calls from Host and also sending
 *         notification to phosphor-software-manager app
 */
class CodeUpdate
{
  public:
    /** @brief Constructor to create an inband codeupdate object
     *  @param[in] dBusIntf - D-Bus handler pointer
     */
    CodeUpdate(const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {
        currBootSide = Tside;
        nextBootSide = Tside;
    }

    /* @brief Method to return the current boot side
     */
    std::string fetchCurrentBootSide();

    /* @brief Method to return the next boot side
     */
    std::string fetchNextBootSide();

    /* @brief Method to set the current boot side or
     *        perform a rename operation on current boot side
     * @param[in] currSide - current side to be set to
     * @return PLDM_SUCCESS codes
     */
    int setCurrentBootSide(const std::string& currSide);

    /* @brief Method to set the next boot side
     * @param[in] nextSide - next boot side to be set to
     * @return PLDM_SUCCESS codes
     */
    int setNextBootSide(const std::string& nextSide);

    /* @brief Method to set the running and non-running
     *        images
     */
    virtual void setVersions();

    /* @brief Method to return the newly upoaded image id in
     *        /tmp
     */
    std::string fetchnewImageId()
    {
        return newImageId;
    }

    /* @brief Method to set the oem platform handler in CodeUpdate class */
    void setOemPlatformHandler(pldm::responder::oem_platform::Handler* handler);

    virtual ~CodeUpdate()
    {}

  private:
    std::string currBootSide;      //!< current boot side
    std::string nextBootSide;      //!< next boot side
    std::string runningVersion;    //!< currently running image
    std::string nonRunningVersion; //!< alternate image
    std::string newImageId;        //!< new image id
    bool codeUpdateInProgress =
        false; //!< indicates whether codeupdate is going on
    const pldm::utils::DBusHandler* dBusIntf; //!< D-Bus handler
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>>
        captureNextBootSideChange; //!< vector to catch the D-Bus property
                                   //!< change for next boot side
    std::unique_ptr<sdbusplus::bus::match::match>
        fwUpdateMatcher; //!< pointer to capture the interface added signal for
                         //!< new image
    pldm::responder::oem_platform::Handler*
        oemPlatformHandler; //!< oem platform handler

    /* @brief Method to take action when the subscribed D-Bus property is
     *        changed
     * @param[in] chProperties - list of properties which have changed
     * @return - none
     */

    void
        processPriorityChangeNotification(const DbusChangedProps& chProperties);
};

/* @brief Method to fetch current or next boot side
 * @param[in] entityInstance - entity instance for the sensor
 * @param[in] codeUpdate - pointer to the CodeUpdate object
 *
 * @return - boot side
 */
uint8_t fetchBootSide(uint16_t entityInstance, CodeUpdate* codeUpdate);

/* @brief Method to set current or next  boot side
 * @param[in] entityInstance - entity instance for the effecter
 * @param[in] currState - state to be set
 * @param[in] stateField - state field set as sent by Host
 * @return - PLDM_SUCCESS codes
 */
int setBootSide(uint16_t entityInstance, uint8_t currState,
                const std::vector<set_effecter_state_field>& stateField,
                CodeUpdate* codeUpdate);

} // namespace responder
} // namespace pldm
