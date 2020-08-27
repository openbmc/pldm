#pragma once
#include "common/utils.hpp"
#include "libpldmresponder/pdr_utils.hpp"
//#include "libpldmresponder/platform.hpp"
#include <string>

namespace pldm
{
namespace responder
{

static constexpr auto P_SIDE = 1;
static constexpr auto T_SIDE = 2;

class CodeUpdate
{
  public:
    CodeUpdate(const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {
        currBootSide = "T";
        nextBootSide = "T";
        runningVersion = "";
        nonRunningVersion = "";
        //setVersions();
    }

    std::string fetchCurrentBootSide();
    std::string fetchNextBootSide();
    bool persistBootSideInfo(); // may be not needed default is T when pldm
                                // starts
    int setCurrentBootSide(std::string currSide);
    int setNextBootSide(std::string nextSide);

    // subscribe here for dbus prop change notification for nonRunningVersion

  private:
    void setVersions();
    std::string currBootSide;
    std::string nextBootSide;
    std::string runningVersion; // verify whether really needed to be maintained
    std::string nonRunningVersion; // empty means single image is installed
    inline static bool codeUpdateInProgress = false;
    const pldm::utils::DBusHandler* dBusIntf;
};

//void buildAllCodeUpdateEffecterPDR(uint16_t nextEffecterId, 
  //                                 pdr_utils::RepoInterface& repo);
  //void buildAllCodeUpdateEffecterPDR(platform::Handler* platformHandler,
   //                                  pdr_utils::RepoInterface& repo);
//void buildAllCodeUpdateSensorPDR(uint16_t nextSensorId, 
  //                               pdr_utils::RepoInterface& repo);
  void buildAllCodeUpdateSensorPDR(pdr_utils::RepoInterface& repo);
} // namespace responder
} // namespace pldm
