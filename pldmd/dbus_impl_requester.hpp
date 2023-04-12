#pragma once

#include "libpldm/instance-id.h"
#include "libpldm/pldm.h"

#include "xyz/openbmc_project/PLDM/Requester/server.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>

#include <map>
namespace pldm
{
namespace dbus_api
{

using RequesterIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PLDM::server::Requester>;

/** @class Requester
 *  @brief OpenBMC PLDM.Requester implementation.
 *  @details A concrete implementation for the
 *  xyz.openbmc_project.PLDM.Requester DBus APIs.
 */
class Requester : public RequesterIntf
{
  public:
    Requester() = delete;
    Requester(const Requester&) = delete;
    Requester& operator=(const Requester&) = delete;
    Requester(Requester&&) = delete;
    Requester& operator=(Requester&&) = delete;
    virtual ~Requester() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    Requester(sdbusplus::bus_t& bus, const std::string& path) :
        RequesterIntf(bus, path.c_str())
    {
        int rc;

        rc = pldm_instance_db_init_default(&pldmInstanceIdDb);
        if (rc < 0)
        {
            throw std::exception();
        }
    };

    /* Ideally we would be able to look up the TID for a given EID. We don't
     * have that infrastructure in place yet. So use the EID value for the TID.
     * This is an interim step towards the PLDM requester logic moving into
     * libpldm, and eventually this won't be needed. */

    /** @brief Implementation for RequesterIntf.GetInstanceId */
    uint8_t getInstanceId(uint8_t eid) override;

    /** @brief Mark an instance id as unused
     *  @param[in] eid - MCTP eid to which this instance id belongs
     *  @param[in] instanceId - PLDM instance id to be freed
     *  @note will throw std::out_of_range if instanceId > 31
     */
    void markFree(uint8_t eid, uint8_t instanceId)
    {
        int rc = pldm_instance_id_free(pldmInstanceIdDb, eid, instanceId);
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }

  private:
    struct pldm_instance_id* pldmInstanceIdDb = NULL;
};

} // namespace dbus_api
} // namespace pldm
