#pragma once

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "terminus.hpp"
#include "terminus_manager.hpp"

namespace pldm
{

namespace platform_mc
{

/**
 * @brief PlatformManager
 *
 * PlatformManager class manages the actions outlined in the platform spec.
 */
class PlatformManager
{
  public:
    PlatformManager() = delete;
    PlatformManager(const PlatformManager&) = delete;
    PlatformManager(PlatformManager&&) = delete;
    PlatformManager& operator=(const PlatformManager&) = delete;
    PlatformManager& operator=(PlatformManager&&) = delete;
    ~PlatformManager() = default;

    explicit PlatformManager(TerminusManager& terminusManager,
                             TerminiMapper& termini) :
        terminusManager(terminusManager),
        termini(termini)
    {}

    /** @brief Initialize terminus which supports PLDM Type 2
     *
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> initTerminus();

  private:
    /** reference of TerminusManager for sending PLDM request to terminus*/
    TerminusManager& terminusManager;

    /** @brief Managed termini list */
    TerminiMapper& termini;
};
} // namespace platform_mc
} // namespace pldm
