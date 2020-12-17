#pragma once

#include "libpldm/platform.h"

#include "pldmd/dbus_impl_requester.hpp"

#include <filesystem>
#include <fstream>
#include <map>

using namespace pldm::dbus_api;

namespace pldm
{
namespace oem_ibm
{
namespace requester
{

/** @class DbusToFileHandler
 *  @brief This class can process resource dump parameters and send PLDM
 *         new file available cmd to the hypervisor
 */
class DbusToFileHandler
{
  public:
    DbusToFileHandler();
    DbusToFileHandler(const DbusToFileHandler&) = delete;
    DbusToFileHandler(DbusToFileHandler&&) = delete;
    DbusToFileHandler& operator=(const DbusToFileHandler&) = delete;
    DbusToFileHandler& operator=(DbusToFileHandler&&) = delete;
    ~DbusToFileHandler() = default;

    /** @brief Constructor
     *  @param[in] mctp_fd - fd of MCTP communications socket
     *  @param[in] mctp_eid - MCTP EID of host firmware
     *  @param[in] requester - reference to Requester object
     */
    explicit DbusToFileHandler(int mctp_fd, uint8_t mctp_eid,
                               Requester& requester);

    int processNewResourceDump();

  private:
    void sendNewFileAvailableCmd(uint64_t fileSize);

    /** @brief fd of MCTP communications socket */
    int mctp_fd;

    /** @brief MCTP EID of host firmware */
    uint8_t mctp_eid;

    /** @brief reference to Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    Requester& requester;
};

} // namespace requester
} // namespace oem_ibm
} // namespace pldm
