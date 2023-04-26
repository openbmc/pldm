#pragma once

#include "libpldm/instance-id.h"

#include "xyz/openbmc_project/Common/error.hpp"

using namespace sdbusplus::xyz::openbmc_project::Common::Error;

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
        int rc;
        rc = pldm_instance_db_init_default(&pldmInstanceIdDb);
        if (rc)
        {
            throw std::exception();
        }
    }

    InstanceIdDb(const std::string& path)
    {
        int rc;
        rc = pldm_instance_db_init(&pldmInstanceIdDb, path.c_str());
        if (rc)
        {
            throw std::exception();
        }
    }
    ~InstanceIdDb()
    {
        pldm_instance_db_destroy(pldmInstanceIdDb);
    }

    /** @brief Get next unused instance id
     *  @return - PLDM instance id
     */
    uint8_t next(uint8_t eid)
    {
        uint8_t id;
        int rc = pldm_instance_id_alloc(pldmInstanceIdDb, eid, &id);
        if (rc == -EAGAIN)
        {
            throw TooManyResources();
        }
        else if (rc)
        {
            throw std::exception();
        }
        return id;
    }

    /** @brief Mark an instance id as unused
     *  @param[in] eid - the EID the instance ID database is associated with
     *  @param[in] instanceId - PLDM instance id to be freed
     */
    void free(uint8_t eid, uint8_t instanceId)
    {
        int rc = pldm_instance_id_free(pldmInstanceIdDb, eid, instanceId);
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }

  private:
    struct pldm_instance_db* pldmInstanceIdDb = NULL;
};

} // namespace pldm
