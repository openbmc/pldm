#pragma once

#include "libpldm/platform.h"

#include "pldmd/dbus_impl_requester.hpp"

#include <filesystem>
#include <fstream>
#include <map>

using namespace pldm::dbus_api;

namespace pldm
{
namespace requester
{
namespace oem_ibm
{

/** @class DbusToFileHandler
 *  @brief This class can process resource dump parameters and send PLDM
 *         new file available cmd to the hypervisor. This class can be used
 *         as a pldm requester in oem-ibm path.
 */
class DbusToFileHandler
{
  public:
    DbusToFileHandler(const DbusToFileHandler&) = delete;
    DbusToFileHandler(DbusToFileHandler&&) = delete;
    DbusToFileHandler& operator=(const DbusToFileHandler&) = delete;
    DbusToFileHandler& operator=(DbusToFileHandler&&) = delete;
    ~DbusToFileHandler() = default;

    /** @brief Constructor
     *  @param[in] mctp_fd - fd of MCTP communications socket
     *  @param[in] mctp_eid - MCTP EID of host firmware
     *  @param[in] requester - pointer to a Requester object
     */
    DbusToFileHandler(int mctp_fd, uint8_t mctp_eid,
                      dbus_api::Requester* requester);

    /** @brief Process the new resource dump request
     *  @param[in] path - D-bus object path of resource dump entry
     */
    void processNewResourceDump(sdbusplus::message::object_path path);

  private:
    /** @brief Send the new file available command request to hypervisor
     *  @param[in] fileSize - size of the file
     */
    void sendNewFileAvailableCmd(uint64_t fileSize);

    /** @brief fd of MCTP communications socket */
    int mctp_fd;

    /** @brief MCTP EID of host firmware */
    uint8_t mctp_eid;

    /** @brief Pointer to a Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    dbus_api::Requester* requester;

    /** @brief Hold the current resource dump object path */
    sdbusplus::message::object_path resDumpCurrentObjPath;
};

} // namespace oem_ibm
} // namespace requester
} // namespace pldm
