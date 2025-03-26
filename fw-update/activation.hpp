#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Object/Delete/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/server.hpp>
#include <xyz/openbmc_project/Software/ApplyTime/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <string>

namespace pldm
{

namespace fw_update
{

class UpdateManager;

using ActivationIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Activation>;
using ActivationProgressIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::ActivationProgress>;
using DeleteIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Object::server::Delete>;
using UpdateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update>;
using ApplyTimeIntf =
    sdbusplus::xyz::openbmc_project::Software::server::ApplyTime;

/** @class ActivationProgress
 *
 *  Concrete implementation of xyz.openbmc_project.Software.ActivationProgress
 *  D-Bus interface
 */
class ActivationProgress : public ActivationProgressIntf
{
  public:
    /** @brief Constructor
     *
     * @param[in] bus - Bus to attach to
     * @param[in] objPath - D-Bus object path
     */
    ActivationProgress(sdbusplus::bus_t& bus, const std::string& objPath) :
        ActivationProgressIntf(bus, objPath.c_str(),
                               action::emit_interface_added)
    {
        progress(0);
    }
};

/** @class Delete
 *
 *  Concrete implementation of xyz.openbmc_project.Object.Delete D-Bus interface
 */
class Delete : public DeleteIntf
{
  public:
    /** @brief Constructor
     *
     *  @param[in] bus - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] updateManager - Reference to FW update manager
     */
    Delete(sdbusplus::bus_t& bus, const std::string& objPath,
           UpdateManager* updateManager) :
        DeleteIntf(bus, objPath.c_str(), action::emit_interface_added),
        updateManager(updateManager)
    {}

    /** @brief Delete the Activation D-Bus object for the FW update package */
    void delete_() override;

  private:
    UpdateManager* updateManager;
};

/** @class Activation
 *
 *  Concrete implementation of xyz.openbmc_project.Object.Activation D-Bus
 *  interface
 */
class Activation : public ActivationIntf
{
  public:
    /** @brief Constructor
     *
     *  @param[in] bus - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] updateManager - Reference to FW update manager
     */
    Activation(sdbusplus::bus_t& bus, std::string objPath,
               Activations activationStatus, UpdateManager* updateManager) :
        ActivationIntf(bus, objPath.c_str(),
                       ActivationIntf::action::defer_emit),
        bus(bus), objPath(objPath), updateManager(updateManager)
    {
        activation(activationStatus);
        deleteImpl = std::make_unique<Delete>(bus, objPath, updateManager);
        emit_object_added();
    }

    using sdbusplus::xyz::openbmc_project::Software::server::Activation::
        activation;
    using sdbusplus::xyz::openbmc_project::Software::server::Activation::
        requestedActivation;

    /** @brief Overriding Activation property setter function
     */
    Activations activation(Activations value) override;

    /** @brief Overriding RequestedActivations property setter function
     */
    RequestedActivations requestedActivation(
        RequestedActivations value) override
    {
        if ((value == RequestedActivations::Active) &&
            (requestedActivation() != RequestedActivations::Active))
        {
            if ((ActivationIntf::activation() == Activations::Ready))
            {
                activation(Activations::Activating);
            }
        }
        return ActivationIntf::requestedActivation(value);
    }

  private:
    sdbusplus::bus_t& bus;
    const std::string objPath;
    UpdateManager* updateManager;
    std::unique_ptr<Delete> deleteImpl;
};

/** @class Update
 *
 *  Concrete implementation of xyz.openbmc_project.Software.Update D-Bus
 *  interface
 */
class Update : public UpdateIntf
{
  public:
    /** @brief Constructor
     *
     *  @param[in] bus - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] updateManager - Reference to FW update manager
     */
    Update(sdbusplus::bus::bus& bus, const std::string& path,
           std::shared_ptr<UpdateManager> updateManager) :
        UpdateIntf(bus, path.c_str()), updateManager(updateManager),
        objPath(path)
    {}

    virtual sdbusplus::message::object_path startUpdate(
        sdbusplus::message::unix_fd image,
        ApplyTimeIntf::RequestedApplyTimes applyTime) override;

    ~Update() noexcept override = default;

  private:
    std::shared_ptr<UpdateManager> updateManager;
    const std::string objPath;
    std::stringstream imageStream;
};

} // namespace fw_update

} // namespace pldm
