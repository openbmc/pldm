#pragma once

#include "common/utils.hpp"

#include <string>

using namespace pldm::utils;
namespace pldm
{
namespace responder
{

static constexpr auto P_SIDE = 1;
static constexpr auto T_SIDE = 2;
static constexpr auto Pside = "P";
static constexpr auto Tside = "T";

class CodeUpdate
{
  public:
    CodeUpdate(const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {
        currBootSide = Tside;
        nextBootSide = Tside;
        runningVersion = "";
        nonRunningVersion = "";
        setVersions();
    }

    std::string fetchCurrentBootSide();
    std::string fetchNextBootSide();
    bool persistBootSideInfo(); // may be not needed default is T when pldm
                                // starts
    int setCurrentBootSide(std::string currSide);
    int setNextBootSide(std::string nextSide);

    // subscribe here for dbus prop change notification for nonRunningVersion
    // for next boot side when set via gui or redfish
    //
    // also subscribe for InterfacesAdded to know if a new image is installed
    // for non running version on xyz.openbmc_project.Software.BMC.Updater
    // /xyz/openbmc_project/software
    //  even if i do not subscribe for the interface added, it is fine
    //  because after bmc reboot pldm will restart and read both the versions
    //  while coming up

  private:
    void setVersions();
    std::string currBootSide;
    std::string nextBootSide;
    std::string runningVersion; // verify whether really needed to be maintained
    std::string nonRunningVersion; // empty means single image is installed
    inline static bool codeUpdateInProgress = false;
    const pldm::utils::DBusHandler* dBusIntf;
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>>
        captureNextBootSideChange;

    void
        processPriorityChangeNotification(const DbusChangedProps& chProperties);
};

uint8_t fetchBootSide(uint16_t entityInstance, CodeUpdate& codeUpdate);

int setBootSide(uint16_t entityInstance, uint8_t currState,
                const std::vector<set_effecter_state_field>& stateField,
                CodeUpdate& codeUpdate);

} // namespace responder
} // namespace pldm
