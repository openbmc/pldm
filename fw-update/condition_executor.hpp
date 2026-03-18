#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <future>

namespace pldm::fw_update
{

using namespace pldm;
using Context = sdbusplus::async::context;

namespace sdbusRule = sdbusplus::bus::match::rules;

constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_ROOT = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";
const auto SYSTEMD_JOB_REMOVED_EVENT =
    sdbusRule::type::signal() + sdbusRule::path(SYSTEMD_ROOT) +
    sdbusRule::interface(SYSTEMD_INTERFACE) + sdbusRule::member("JobRemoved");

class ConditionExecutor
{
  public:
    ConditionExecutor() = delete;
    ConditionExecutor(const ConditionExecutor&) = delete;
    ConditionExecutor(ConditionExecutor&&) = delete;
    ConditionExecutor& operator=(const ConditionExecutor&) = delete;
    ConditionExecutor& operator=(ConditionExecutor&&) = delete;
    ~ConditionExecutor() = default;

    explicit ConditionExecutor(sdbusplus::bus_t& bus,
                               const ConditionPath& conditionPath,
                               const std::string& args);

    void executeAndWait();

    void execute();

    void handleSystemdJobRemoved(sdbusplus::message_t& msg);

  private:
    std::string serviceName();

    sdbusplus::bus_t& bus;

    const std::string& conditionPath;

    const std::string& args;

    sdbusplus::message::object_path jobPath;

    std::promise<void> promise;

    sdbusplus::bus::match_t systemdSignals;
};

} // namespace pldm::fw_update
