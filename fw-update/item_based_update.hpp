
#pragma once
#include <xyz/openbmc_project/Software/ApplyTime/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
namespace pldm::fw_update
{
class ItemBasedUpdateManager;
using UpdateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update>;
using ApplyTimeIntf =
    sdbusplus::xyz::openbmc_project::Software::server::ApplyTime;

class ItemBasedUpdate : public UpdateIntf
{
  public:
    /**
     * @brief Constructor for ItemBasedUpdate
     *
     * @param[in] bus The D-Bus bus object
     * @param[in] path The object path for the D-Bus interface
     * @param[in] updateManager Pointer to the ItemBasedUpdateManager
     */
    ItemBasedUpdate(sdbusplus::bus::bus& bus, const std::string& path,
                    ItemBasedUpdateManager* updateManager) :
        UpdateIntf(bus, path.c_str()), updateManager(updateManager),
        objPath(path)
    {}

    /**
     * @brief Start the update process
     *
     * D-Bus method implementation for starting the update process
     *
     * @param[in] image The image file descriptor
     * @param[in] applyTime The requested apply time
     */
    virtual sdbusplus::message::object_path startUpdate(
        sdbusplus::message::unix_fd image,
        ApplyTimeIntf::RequestedApplyTimes applyTime =
            ApplyTimeIntf::RequestedApplyTimes::Immediate) override;
    ~ItemBasedUpdate() noexcept override = default;

  private:
    /**
     * @brief Pointer to the ItemBasedUpdateManager
     */
    ItemBasedUpdateManager* updateManager;

    /**
     * @brief The D-Bus object path for this interface
     */
    const std::string objPath;

    /**
     * @brief The firmware package data
     */
    std::span<const uint8_t> package;
};
} // namespace pldm::fw_update
