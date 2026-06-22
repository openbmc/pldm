#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <sdbusplus/async.hpp>
#include <sdbusplus/server/object.hpp>

#include <functional>
#include <map>

namespace pldm::fw_update
{

namespace sdbusRule = sdbusplus::bus::match::rules;

class SystemdInterface
{
  public:
    using TaskCallback = std::function<void(bool)>;

    SystemdInterface() = delete;
    SystemdInterface(const SystemdInterface&) = delete;
    SystemdInterface(SystemdInterface&&) = delete;
    SystemdInterface& operator=(const SystemdInterface&) = delete;
    SystemdInterface& operator=(SystemdInterface&&) = delete;
    ~SystemdInterface() = default;

    static SystemdInterface& getInstance(sdbusplus::bus_t& bus);

    void execute(const ConditionPath& conditionPath, const std::string& args,
                 TaskCallback taskCallback);

    void handleSystemdJobRemoved(sdbusplus::message_t& msg);

  private:
    explicit SystemdInterface(sdbusplus::bus_t& bus);

    std::string serviceName(const ConditionPath& conditionPath,
                            const std::string& args) const;

    sdbusplus::bus_t& bus;

    sdbusplus::bus::match_t systemdSignals;

    std::map<sdbusplus::object_path, TaskCallback> taskCallbacks;

    std::vector<sdbusplus::slot_t> asyncSlots;
};

} // namespace pldm::fw_update
