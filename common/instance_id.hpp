#pragma once

#include <libpldm/instance-id.h>

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <cstdint>
#include <exception>
#include <string>
#include <system_error>

namespace pldm
{

class InstanceId
{
    pldm_instance_db* pldmInstanceIdDb = nullptr;
    uint8_t tid_ = 0xff;
    uint8_t iid_ = 0xff;

  public:
    InstanceId() = delete;
    InstanceId(pldm_instance_db* db, uint8_t tid) :
        pldmInstanceIdDb(db), tid_(tid)
    {
        int rc = pldm_instance_id_alloc(pldmInstanceIdDb, tid, &iid_);

        if (rc == -EAGAIN)
        {
            throw std::runtime_error("No free instance ids");
        }

        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }
    InstanceId(const InstanceId& other) = delete;
    InstanceId(InstanceId&& other)
    {
        iid_ = other.iid_;
        tid_ = other.tid_;
        other.iid_ = other.tid_ = 0xff;
    }
    InstanceId& operator=(const InstanceId& other) = delete;
    InstanceId& operator=(InstanceId&& other)
    {
        iid_ = other.iid_;
        tid_ = other.tid_;
        other.iid_ = other.tid_ = 0xff;
        return *this;
    }
    ~InstanceId()
    {
        if (iid_ != 0xff && tid_ != 0xff)
        {
            int rc = pldm_instance_id_free(pldmInstanceIdDb, tid_, iid_);
            if (rc == -EINVAL)
            {
                lg2::error(
                    "Instance ID {IID} for TID {TID} was not previously allocated",
                    "IID", iid_, "TID", tid_);
            }
            else if (rc)
            {
                lg2::error(
                    "Failed freeing Instance ID {IID} for TID {TID} with error: {ERROR}",
                    "IID", iid_, "TID", tid_, "ERROR", rc);
            }
        }
    }
};

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

    /** @brief Constructor
     *
     *  @param[in] path - instance ID database path
     */
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
        /*
         * Abandon error-reporting. We shouldn't throw an exception from the
         * destructor, and the class has multiple consumers using incompatible
         * logging strategies.
         *
         * Broadly, it should be possible to use strace to investigate.
         */
        pldm_instance_db_destroy(pldmInstanceIdDb);
    }

    /** @brief Allocate an instance ID for the given terminus
     *  @param[in] tid - the terminus ID the instance ID is associated with
     *  @return - PLDM instance id or -EAGAIN if there are no available instance
     *            IDs
     */
    uint8_t next(uint8_t tid)
    {
        uint8_t id;
        int rc = pldm_instance_id_alloc(pldmInstanceIdDb, tid, &id);

        if (rc == -EAGAIN)
        {
            throw std::runtime_error("No free instance ids");
        }

        if (rc)
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
        if (rc == -EINVAL)
        {
            throw std::runtime_error(
                "Instance ID " + std::to_string(instanceId) + " for TID " +
                std::to_string(tid) + " was not previously allocated");
        }
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }
    InstanceId get(uint8_t tid)
    {
        return InstanceId(pldmInstanceIdDb, tid);
    }

  private:
    pldm_instance_db* pldmInstanceIdDb = nullptr;
};

} // namespace pldm
