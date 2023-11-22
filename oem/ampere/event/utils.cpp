#include "utils.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace oem
{

constexpr auto faultLogBusName = "xyz.openbmc_project.Dump.Manager";
constexpr auto faultLogPath = "/xyz/openbmc_project/dump/faultlog";
constexpr auto faultLogIntf = "xyz.openbmc_project.Dump.Create";
constexpr auto logBusName = "xyz.openbmc_project.Logging.IPMI";
constexpr auto logPath = "/xyz/openbmc_project/Logging/IPMI";
constexpr auto logIntf = "xyz.openbmc_project.Logging.IPMI";
static time_t prevTs = 0;
static int indexId = 0;

std::string getUniqueEntryID(std::string& prefix)
{
    // Get the entry timestamp
    std::time_t curTs = time(0);
    std::tm timeStruct = *std::localtime(&curTs);
    char buf[80];
    // If the timestamp isn't unique, increment the index
    if (curTs == prevTs)
    {
        indexId++;
    }
    else
    {
        // Otherwise, reset it
        indexId = 0;
    }
    // Save the timestamp
    prevTs = curTs;
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &timeStruct);
    std::string uniqueId(buf);
    uniqueId = prefix + uniqueId;
    if (indexId > 0)
    {
        uniqueId += "_" + std::to_string(indexId);
    }
    return uniqueId;
}

void addFaultLogToRedfish(std::string& primaryLogId, std::string& type)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::map<std::string, std::variant<std::string, uint64_t>> params;

    params["Type"] = type;
    params["PrimaryLogId"] = primaryLogId;
    try
    {
        auto method = bus.new_method_call(faultLogBusName, faultLogPath,
                                          faultLogIntf, "CreateDump");
        method.append(params);

        auto response = bus.call(method);
        if (response.is_method_error())
        {
            error("createDump response error");
        }
    }
    catch (const std::exception& e)
    {
        error("call createDump error - {ERROR}", "ERROR", e);
    }
}

void addOEMSelLog(std::string& msg, std::vector<uint8_t>& evtData,
                  uint8_t recordType)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        auto method = bus.new_method_call(logBusName, logPath, logIntf,
                                          "IpmiSelAddOem");
        method.append(msg, evtData, recordType);

        auto selReply = bus.call(method);
        if (selReply.is_method_error())
        {
            error("add SEL log error");
        }
    }
    catch (const std::exception& e)
    {
        error("call SEL log error - {ERROR}", "ERROR", e);
    }
}

} // namespace oem
} // namespace pldm
