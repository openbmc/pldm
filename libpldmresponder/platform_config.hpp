#pragma once
#include "common/utils.hpp"
#include "pldmd/handler.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>

PHOSPHOR_LOG2_USING;

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

class Handler : public CmdHandler
{
  public:
    Handler()
    {
        systemCompatibleMatchCallBack =
            std::make_unique<sdbusplus::bus::match_t>(
                pldm::utils::DBusHandler::getBus(),
                sdbusplus::bus::match::rules::interfacesAdded() +
                    sdbusplus::bus::match::rules::sender(
                        "xyz.openbmc_project.EntityManager"),
                std::bind(&Handler::systemCompatibleCallback, this,
                          std::placeholders::_1));
    }

    /** @brief Interface to get the system type information
     *
     *  @return - the system type information
     */
    virtual std::optional<std::filesystem::path> getPlatformName();

    /** @brief D-Bus Interface added signal match for Entity Manager */
    void systemCompatibleCallback(sdbusplus::message_t& msg);

  private:
    /** @brief system type/model */
    std::string systemType;

    /** @brief D-Bus Interface added signal match for Entity Manager */
    std::unique_ptr<sdbusplus::bus::match_t> systemCompatibleMatchCallBack;
};

} // namespace platform_config

} // namespace responder

} // namespace pldm
