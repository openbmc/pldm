#pragma once
#include "common/utils.hpp"
#include "pldmd/handler.hpp"

namespace pldm
{
namespace responder
{

namespace platform_config
{
using namespace pldm::utils;

static constexpr auto compatibleInterface =
    "xyz.openbmc_project.Inventory.Decorator.Compatible";
static constexpr auto namesProperty = "Names";

using SystemTypeCallback = std::function<void(const std::string&, bool)>;

class Handler : public CmdHandler
{
  public:
    Handler(const std::string pdrPath = "") : pdrDirPath(pdrPath)
    {
        systemCompatibleMatchCallBack =
            std::make_unique<sdbusplus::bus::match_t>(
                pldm::utils::DBusHandler::getBus(),
                sdbusplus::bus::match::rules::interfacesAdded() +
                    sdbusplus::bus::match::rules::sender(
                        "xyz.openbmc_project.EntityManager"),
                std::bind(&Handler::systemCompatibleCallback, this,
                          std::placeholders::_1));
        sysTypeCallback = nullptr;
    }

    /** @brief Interface to get the system type information using Dbus query
     *
     *  @return - the system type information
     */
    virtual std::optional<std::filesystem::path> getPlatformName();

    /** @brief D-Bus Interface added signal match for Entity Manager */
    void systemCompatibleCallback(sdbusplus::message_t& msg);

    /** @brief Registers the callback from other objects */
    void registerSystemTypeCallback(SystemTypeCallback callback);

  private:
    /** @brief Interface to get the first available directory
     *         availble from the received list
     *
     *  @param[in] dirNames - Getting system names from Entity manager
     *  @return - The system type information
     */
    std::string getPDRJsonDir(const std::vector<std::string> dirNames);

    /** @brief system type/model */
    std::string systemType;

    /** @brief D-Bus Interface added signal match for Entity Manager */
    std::unique_ptr<sdbusplus::bus::match_t> systemCompatibleMatchCallBack;

    /** @brief Registered Callback */
    SystemTypeCallback sysTypeCallback;

    /** @brief PDR json file directory path */
    std::string pdrDirPath;
};

} // namespace platform_config

} // namespace responder

} // namespace pldm
