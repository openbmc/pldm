#pragma once

#include "common/utils.hpp"

#include <string>

namespace pldm
{
namespace responder
{

class CodeUpdate
{
  public:
    CodeUpdate(const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {
        currBootSide = "T";
        nextBootSide = "T";
    }

    std::string fetchCurrentBootSide();
    std::string fetchNextBootSide();
    bool persistBootSideInfo(); // may be not needed default is T when pldm
                                // starts
    int setCurrentBootSide(std::string currSide);
    int setNextBootSide(std::string nextSide);

    // subscribe here for dbus prop change notification for nonRunningVersion

  private:
    std::string currBootSide;
    std::string nextBootSide;
    std::string runningVersion; // verify whether really needed to be maintained
    std::string nonRunningVersion;
    inline static bool codeUpdateInProgress = false;
    const pldm::utils::DBusHandler* dBusIntf;
};

} // namespace responder
} // namespace pldm
