#include "systemd_interface.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <format>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_ROOT = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";
constexpr auto SYSTEMD_JOB_RESULT_DONE = "done";

// Characters systemd-escape(1) leaves as they are. '-' is not among them even
// though a unit name may carry it, since it is the escaped form of '/'.
constexpr std::string_view UNIT_INSTANCE_SAFE_CHARS =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    ":_.";

// Marks a systemd template unit, whose instance name carries the arguments
constexpr auto UNIT_TEMPLATE_MARKER = '@';

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
                               TaskCallback&& taskCallback)
{
    if (conditionPath.empty())
    {
        if (taskCallback)
        {
            taskCallback(true);
        }
        return;
    }

    const auto service = conditionUnitName(conditionPath, args);

    releaseCompletedSlots();

    try
    {
        auto method = bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_ROOT,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(service, "replace");

        const auto slotId = nextSlotId++;
        auto callback = [this, slotId, service,
                         taskCallback = std::move(taskCallback)](
                            sdbusplus::message_t&& msg) mutable {
            completedSlots.push_back(slotId);

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

        asyncSlots.insert_or_assign(slotId, method.call_async(callback));
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

void SystemdInterface::releaseCompletedSlots()
{
    for (const auto slotId : completedSlots)
    {
        asyncSlots.erase(slotId);
    }
    completedSlots.clear();
}

std::string escapeUnitInstance(std::string_view arg)
{
    std::string escaped;
    for (const auto c : arg)
    {
        if (c == '/')
        {
            escaped += '-';
        }
        else if (UNIT_INSTANCE_SAFE_CHARS.find(c) != std::string_view::npos &&
                 !(c == '.' && escaped.empty()))
        {
            escaped += c;
        }
        else
        {
            escaped += std::format("\\x{:02x}", static_cast<uint8_t>(c));
        }
    }
    return escaped;
}

std::string conditionUnitName(const ConditionPath& conditionPath,
                              const std::string& args)
{
    // A parameterized condition follows the systemd template naming convention
    // <name>@<suffix>, so the arguments belong between the '@' and the suffix.
    // Anything else takes no arguments.
    const auto suffix = conditionPath.rfind('.');
    if (suffix == std::string::npos || suffix == 0 ||
        conditionPath[suffix - 1] != UNIT_TEMPLATE_MARKER)
    {
        return conditionPath;
    }

    if (args.empty())
    {
        error(
            "No arguments available for the parameterized condition {UNIT}, starting it will fail",
            "UNIT", conditionPath);
        return conditionPath;
    }

    return std::format("{}{}{}", conditionPath.substr(0, suffix),
                       escapeUnitInstance(args), conditionPath.substr(suffix));
}

void SystemdInterface::handleSystemdJobRemoved(sdbusplus::message_t& msg)
{
    releaseCompletedSlots();

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

        const bool success = (result == SYSTEMD_JOB_RESULT_DONE);
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
