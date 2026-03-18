#include "condition_executor.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{
ConditionExecutor::ConditionExecutor(sdbusplus::bus_t& bus,
                                     const ConditionPath& conditionPath,
                                     const std::string& args) :
    bus(bus), conditionPath(conditionPath), args(args),
    systemdSignals(
        bus, SYSTEMD_JOB_REMOVED_EVENT,
        std::bind_front(&ConditionExecutor::handleSystemdJobRemoved, this))
{}

void ConditionExecutor::execute()
{
    if (conditionPath.empty())
    {
        return;
    }
    auto method = bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_ROOT,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceName(), "replace");
    auto reply = bus.call(method);
    reply.read(jobPath);
    debug("Started Service: {SERV}", "SERV", serviceName());
    debug("Job path: {JOB}", "JOB", jobPath);
}

std::string ConditionExecutor::serviceName()
{
    auto service = conditionPath;
    if (!args.empty())
    {
        auto lastSlash = args.rfind('/');
        if (lastSlash == args.size() - 1)
        {
            error("Bad argument to a service unit: {ARG}", "ARG", args);
            throw(std::runtime_error("Bad argument to a service unit"));
        }
        auto subFrom = lastSlash == std::string::npos ? 0 : lastSlash + 1;
        auto prunedArg = args.substr(subFrom);
        service += "@" + prunedArg;
    }
    service += ".service";
    return service;
}

void ConditionExecutor::handleSystemdJobRemoved(sdbusplus::message_t& msg)
{
    uint32_t jobId;
    sdbusplus::message::object_path returnedJobPath;
    try
    {
        msg.read(jobId, returnedJobPath);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to read JobRemoved signal: {ERROR}", "ERROR", e);
        return;
    }

    if (returnedJobPath == jobPath)
    {
        debug("Condition execution completed for service: {SERV}", "SERV",
              serviceName());
        promise.set_value();
    }
}

void ConditionExecutor::executeAndWait()
{
    std::future<void> future = promise.get_future();
    execute();
    future.wait();
    return;
}

} // namespace pldm::fw_update
