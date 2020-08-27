#pragma once
#include "common/utils.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "libpldmresponder/platform.hpp"
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
        std::cout << "enter CodeUpdate constructor \n";
        currBootSide = Tside;
        nextBootSide = Tside;
        std::cout << "set currBootSide= " << currBootSide.c_str() << "\n";
        runningVersion = "";
        nonRunningVersion = "";
        // setVersions();
        std::cout << "exit CodeUpdate constructor \n";
    }

    std::string fetchCurrentBootSide();
    std::string fetchNextBootSide();
    bool persistBootSideInfo(); // may be not needed default is T when pldm
                                // starts
    int setCurrentBootSide(const std::string& currSide);
    int setNextBootSide(const std::string& nextSide);

    virtual void setVersions();

    virtual ~CodeUpdate()
    {}
    bool isCodeUpdateInProgress()
    {
        return codeUpdateInProgress;
    }

    void setCodeUpdateProgress(bool progress)
    {
        codeUpdateInProgress = progress;
    }

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
    std::string currBootSide;
    std::string nextBootSide;
    std::string runningVersion; // verify whether really needed to be maintained
    std::string nonRunningVersion; // empty means single image is installed
    bool codeUpdateInProgress = false;
    const pldm::utils::DBusHandler* dBusIntf;
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>>
        captureNextBootSideChange;

    void
        processPriorityChangeNotification(const DbusChangedProps& chProperties);
};

uint8_t fetchBootSide(uint16_t entityInstance, CodeUpdate* codeUpdate);

int setBootSide(uint16_t entityInstance, uint8_t currState,
                const std::vector<set_effecter_state_field>& stateField,
                CodeUpdate* codeUpdate);

void buildAllCodeUpdateEffecterPDR(platform::Handler* platformHandler,
                                   pdr_utils::RepoInterface& repo);
void buildAllCodeUpdateSensorPDR(platform::Handler* platformHandler,
                                 pdr_utils::RepoInterface& repo);
} // namespace responder
} // namespace pldm
