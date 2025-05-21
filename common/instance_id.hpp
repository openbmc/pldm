#pragma once

#include <config.h>
#include <libpldm/instance-id.h>

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <expected>
#include <mutex>
#include <string>
#include <system_error>

PHOSPHOR_LOG2_USING;

namespace pldm
{

/**
 * @class InstanceIdError
 * @brief Exception for PLDM instance ID allocation and management errors.
 */
class InstanceIdError : public std::exception
{
  public:
    InstanceIdError(const InstanceIdError&) noexcept = default;
    InstanceIdError(InstanceIdError&&) noexcept = default;
    InstanceIdError& operator=(const InstanceIdError&) noexcept = default;
    InstanceIdError& operator=(InstanceIdError&&) noexcept = default;
    ~InstanceIdError() noexcept override = default;

    /** @brief Construct with an error code. */
    explicit InstanceIdError(int rc) : rc_(rc), msg_(rcToMsg(rc)) {}

    /** @brief Construct with an error code and a string message (copied). */
    InstanceIdError(int rc, const std::string& m) : rc_(rc), msg_(m) {}

    /** @brief Construct with an error code and a string message (moved). */
    InstanceIdError(int rc, std::string&& m) : rc_(rc), msg_(std::move(m)) {}

    /** @brief Get the error code. */
    int rc() const noexcept
    {
        return rc_;
    }

    /** @brief Get the error message. */
    const std::string& msg() const noexcept
    {
        return msg_;
    }

    /** @brief Convert an error code to a message. */
    static std::string rcToMsg(int rc)
    {
        switch (rc)
        {
            case -EAGAIN:
                return "No free instance ids";
            default:
                return std::system_category().message(rc);
        }
    }

    /** @brief Get the error message (for std::exception). */
    const char* what() const noexcept override
    {
        return msg_.c_str();
    }

  private:
    int rc_;
    std::string msg_;
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
     *  @return - PLDM instance id on success, or InstanceIdError on failure
     */
    std::expected<uint8_t, InstanceIdError> next(
        uint8_t tid, uint32_t timeoutMs = INSTANCEID_ALLOCATION_TIME_OUT)
    {
        std::unique_lock<std::mutex> lk(dbMutex);
        auto timeout = std::chrono::system_clock::now() +
                       std::chrono::milliseconds(timeoutMs);
        uint8_t id = 0xff;
        int rc = 0;
        bool done = dbEvent.wait_until(lk, timeout, [&]() {
            return pldm_instance_id_alloc(pldmInstanceIdDb, tid, &id) == 0;
        });
        if (!done || id == 0xff)
        {
            // Try to allocate again just one more time.
            rc = pldm_instance_id_alloc(pldmInstanceIdDb, tid, &id);
            if (rc)
            {
                std::string msg =
                    "Failed to allocate instance ID for EID " +
                    std::to_string(tid) + ": " + InstanceIdError::rcToMsg(rc);
                error("Instance ID allocation failed for EID {EID}: {MSG}",
                      "EID", tid, "MSG", msg);
                return std::unexpected(InstanceIdError{rc, std::move(msg)});
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
