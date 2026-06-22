#include "systemd_interface.hpp"

#include <phosphor-logging/lg2.hpp>

#include <stdexcept>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_ROOT = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";

const auto SYSTEMD_JOB_REMOVED_EVENT =
    sdbusRule::type::signal() + sdbusRule::path(SYSTEMD_ROOT) +
    sdbusRule::interface(SYSTEMD_INTERFACE) + sdbusRule::member("JobRemoved");

SystemdInterface::SystemdInterface(sdbusplus::bus_t& bus) :
    bus(bus),
    systemdSignals(
        bus, SYSTEMD_JOB_REMOVED_EVENT,
        std::bind_front(&SystemdInterface::handleSystemdJobRemoved, this))
{}

SystemdInterface& SystemdInterface::getInstance(sdbusplus::bus_t& bus)
{
    static SystemdInterface instance(bus);
    return instance;
}

void SystemdInterface::execute(const ConditionPath& conditionPath,
                               const std::string& args,
                               TaskCallback taskCallback)
{
    if (conditionPath.empty())
    {
        if (taskCallback)
        {
            taskCallback(true);
        }
        return;
    }

    std::string service;
    try
    {
        service = serviceName(conditionPath, args);
    }
    catch (const std::exception& e)
    {
        error("Failed to form service name: {ERROR}", "ERROR", e);
        if (taskCallback)
        {
            taskCallback(false);
        }
        return;
    }

    try
    {
        auto method = bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_ROOT,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(service, "replace");

        auto callback = [this, service, taskCallback = std::move(taskCallback)](
                            sdbusplus::message_t&& msg) mutable {
            if (msg.is_method_error())
            {
                error("Failed to start service {SERV}: {ERROR}", "SERV",
                      service, "ERROR", msg.get_error()->message);
                if (taskCallback)
                {
                    taskCallback(false);
                }
                return;
            }

            try
            {
                sdbusplus::object_path jobPath;
                msg.read(jobPath);

                if (taskCallback)
                {
                    taskCallbacks.insert_or_assign(jobPath,
                                                   std::move(taskCallback));
                }
                debug("Started Service: {SERV}", "SERV", service);
                debug("Job path: {JOB}", "JOB", jobPath);
            }
            catch (const std::exception& e)
            {
                error("Failed to parse StartUnit reply: {ERROR}", "ERROR", e);
                if (taskCallback)
                {
                    taskCallback(false);
                }
            }
        };

        [[maybe_unused]] auto slot = method.call_async(callback);
    }
    catch (const std::exception& e)
    {
        error("Failed to issue StartUnit for service {SERV}: {ERROR}", "SERV",
              service, "ERROR", e);
        if (taskCallback)
        {
            taskCallback(false);
        }
    }
}

std::string SystemdInterface::serviceName(const ConditionPath& conditionPath,
                                          const std::string& args) const
{
    auto service = conditionPath;
    if (!args.empty())
    {
        auto lastSlash = args.rfind('/');
        if (lastSlash == args.size() - 1)
        {
            error("Bad argument to a service unit: {ARG}", "ARG", args);
            throw std::runtime_error("Bad argument to a service unit");
        }
        auto subFrom = lastSlash == std::string::npos ? 0 : lastSlash + 1;
        auto prunedArg = args.substr(subFrom);
        service += "@" + prunedArg;
    }
    service += ".service";
    return service;
}

void SystemdInterface::handleSystemdJobRemoved(sdbusplus::message_t& msg)
{
    uint32_t jobId;
    sdbusplus::object_path returnedJobPath;
    std::string unit;
    std::string result;
    try
    {
        msg.read(jobId, returnedJobPath, unit, result);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to read JobRemoved signal: {ERROR}", "ERROR", e);
        return;
    }

    auto it = taskCallbacks.find(returnedJobPath);
    if (it != taskCallbacks.end())
    {
        auto taskCallback = std::move(it->second);
        taskCallbacks.erase(it);

        const bool success = (result == "done");
        debug(
            "Condition execution completed: id={ID}, job={JOB}, unit={UNIT}, result={RESULT}",
            "ID", jobId, "JOB", returnedJobPath, "UNIT", unit, "RESULT",
            result);

        if (taskCallback)
        {
            taskCallback(success);
        }
    }
}

} // namespace pldm::fw_update
