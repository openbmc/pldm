#pragma once

#include "libpldm/instance-id.h"

#include <cerrno>
#include <cstdint>
#include <exception>
#include <string>
#include <system_error>

namespace pldm
{

/** @class InstanceId
 *  @brief Implementation of PLDM instance id as per DSP0240 v1.0.0
 */
class InstanceIdDb
{
  public:
    InstanceIdDb()
    {
        int rc = pldm_instance_db_init_default(&pldmInstanceIdDb);
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }

    InstanceIdDb(const std::string& path)
    {
        int rc = pldm_instance_db_init(&pldmInstanceIdDb, path.c_str());
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }

    ~InstanceIdDb()
    {
        pldm_instance_db_destroy(pldmInstanceIdDb);
    }

    /** @brief Allocate an instance ID for the given terminus
     *  @return - PLDM instance id or -EAGAIN if there are no available instance
     * IDs
     */
    int next(uint8_t tid)
    {
        uint8_t id;
        int rc = pldm_instance_id_alloc(pldmInstanceIdDb, tid, &id);

        if (rc == -EAGAIN)
        {
            return rc;
        }
        else if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }

        return id;
    }

    /** @brief Mark an instance id as unused
     *  @param[in] tid - the terminus ID the instance ID is associated with
     *  @param[in] instanceId - PLDM instance id to be freed
     */
    void free(uint8_t tid, uint8_t instanceId)
    {
        int rc = pldm_instance_id_free(pldmInstanceIdDb, tid, instanceId);
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }

  private:
    struct pldm_instance_db* pldmInstanceIdDb = NULL;
};

} // namespace pldm
