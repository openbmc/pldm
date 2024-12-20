#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;
namespace pldm
{

namespace fw_update
{

using namespace pldm;
namespace sdbusRule = sdbusplus::bus::match::rules;

constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_ROOT = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";
const auto SYSTEMD_JOB_REMOVED_EVENT =
    sdbusRule::type::signal() + sdbusRule::path(SYSTEMD_ROOT) +
    sdbusRule::interface(SYSTEMD_INTERFACE) + sdbusRule::member("JobRemoved");

class FirmwareCondition
{
  public:
    explicit FirmwareCondition(
        const FirmwareConditionPath& condition = FirmwareConditionPath(),
        const std::string& args = std::string(),
        std::unique_ptr<std::function<void()>> postRoutine = nullptr) :
        condition(condition), args(args), postRoutine(std::move(postRoutine)),
        systemdSignals(
            bus, SYSTEMD_JOB_REMOVED_EVENT,
            std::bind_front(std::mem_fn(&FirmwareCondition::unitStateChange),
                            this))
    {}

    void execute(std::unique_ptr<std::function<void()>> postRoutine = nullptr)
    {
        if (postRoutine)
        {
            this->postRoutine = std::move(postRoutine);
        }
        if (condition.empty())
        {
            error("Condition is empty");
            (*this->postRoutine)();
            return;
        }
        auto method = bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_ROOT,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(serviceName(), "replace");
        auto reply = bus.call(method);
        reply.read(jobPath);
        info("Started Service: {SERV}", "SERV", serviceName());
        info("Job path: {JOB}", "JOB", jobPath);
    }

    std::string serviceName()
    {
        auto service = condition;
        if (!args.empty())
        {
            auto lastSlash = args.rfind("/");
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

    void unitStateChange(sdbusplus::message_t& msg)
    {
        uint32_t jobId;
        sdbusplus::message::object_path returnedJobPath;
        try
        {
            msg.read(jobId, returnedJobPath);
        }
        catch (const std::exception& e)
        {
            error("Error reading message, {WHAT}", "WHAT", e.what());
            return;
        }
        if (returnedJobPath == jobPath)
        {
            info("received! {JOB}", "JOB", returnedJobPath);
            if (postRoutine)
            {
                (*postRoutine)();
            }
        }
    }

  private:
    decltype(pldm::utils::DBusHandler::getBus()) bus =
        pldm::utils::DBusHandler::getBus();
    FirmwareConditionPath condition;
    std::string args;
    sdbusplus::message::object_path jobPath;
    std::unique_ptr<std::function<void()>> postRoutine;
    sdbusplus::bus::match_t systemdSignals;
};

} // namespace fw_update
} // namespace pldm
