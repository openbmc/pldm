#pragma once

#include <libpldm/instance-id.h>

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
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

    /** @brief Constructor
     *
     *  @param[in] path - instance ID database path
     */
    InstanceIdDb(const std::string& path) : dbPath(path)
    {
        int rc = pldm_instance_db_init(&pldmInstanceIdDb, path.c_str());
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }
    InstanceIdDb(const InstanceIdDb& other)
    {
        dbPath = other.dbPath;
        int rc =
            dbPath ? pldm_instance_db_init(&pldmInstanceIdDb, (*dbPath).c_str())
                   : pldm_instance_db_init_default(&pldmInstanceIdDb);
        if (rc)
        {
            throw std::system_category().default_error_condition(rc);
        }
    }
    InstanceIdDb(InstanceIdDb&& other) noexcept
    {
        dbPath = std::move(other.dbPath);
        pldmInstanceIdDb = other.pldmInstanceIdDb;
        other.pldmInstanceIdDb = nullptr;
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
     *  @param[in] timeout - [optional] If non-zero the function will block
     *                       for the provided timeout till a instance Id
     *                       is available
     *  @return - PLDM instance id or -EAGAIN if there are no available instance
     *            IDs
     */
    uint8_t next(uint8_t tid, uint32_t timeout = 0)
    {
        std::unique_lock<std::mutex> lk(dbMutex);
        auto timeout_sec =
            std::chrono::system_clock::now() + std::chrono::seconds(timeout);
        uint8_t id = 0xff;
        int rc = 0;
        bool done = dbEvent.wait_until(lk, timeout_sec, [&]() {
            return pldm_instance_id_alloc(pldmInstanceIdDb, tid, &id) == 0;
        });
        if (!done || id == 0xff)
        {
            // Try to allocate again just one more time.
            rc = pldm_instance_id_alloc(pldmInstanceIdDb, tid, &id);
            if (rc == -EAGAIN)
            {
                throw std::runtime_error("No free instance ids");
            }

            if (rc)
            {
                throw std::system_category().default_error_condition(rc);
            }
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
        dbEvent.notify_all();
    }

  private:
    std::optional<std::string> dbPath{};
    pldm_instance_db* pldmInstanceIdDb = nullptr;
    std::mutex dbMutex{};
    std::condition_variable dbEvent{};
};

} // namespace pldm
