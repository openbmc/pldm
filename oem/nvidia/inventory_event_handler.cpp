#include "inventory_event_handler.hpp"

#include "common/utils.hpp"

#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/exception.hpp>

#include <charconv>
#include <string>
#include <system_error>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace oem_nvidia
{

namespace
{
constexpr size_t oemHeaderSize = 4;
constexpr std::string_view processorModulePrefix = "ProcessorModule_";
} // namespace

std::optional<std::string_view> parseInventoryEventPayload(
    const uint8_t* eventData, size_t eventDataSize)
{
    if (eventData == nullptr || eventDataSize < oemHeaderSize)
    {
        return std::nullopt;
    }

    size_t payloadSize = static_cast<size_t>(eventData[2]) |
                         (static_cast<size_t>(eventData[3]) << 8);
    if (payloadSize == 0 || eventDataSize - oemHeaderSize < payloadSize)
    {
        return std::nullopt;
    }

    std::string_view payload(
        reinterpret_cast<const char*>(eventData + oemHeaderSize), payloadSize);

    /* The terminus counts the NUL terminator of the document in the declared
     * size, so it has to come off before the document is passed on.
     */
    while (!payload.empty() && payload.back() == '\0')
    {
        payload.remove_suffix(1);
    }

    if (payload.empty())
    {
        return std::nullopt;
    }

    return payload;
}

std::optional<int32_t> processorModuleIndex(std::string_view terminusName)
{
    if (!terminusName.starts_with(processorModulePrefix))
    {
        return std::nullopt;
    }
    terminusName.remove_prefix(processorModulePrefix.size());

    int32_t index = 0;
    const auto* end = terminusName.data() + terminusName.size();
    auto [ptr, ec] = std::from_chars(terminusName.data(), end, index);
    if (ec != std::errc{} || ptr != end)
    {
        return std::nullopt;
    }

    return index;
}

int handleInventoryEvent(pldm_tid_t tid, std::string_view terminusName,
                         const uint8_t* eventData, size_t eventDataSize)
{
    auto payload = parseInventoryEventPayload(eventData, eventDataSize);
    if (!payload)
    {
        lg2::error(
            "Malformed inventory event from terminus {TID}, event size {SIZE}",
            "TID", tid, "SIZE", eventDataSize);
        return PLDM_ERROR_INVALID_DATA;
    }

    auto moduleIndex = processorModuleIndex(terminusName);
    if (!moduleIndex)
    {
        lg2::error(
            "Inventory event from terminus {TID} named '{NAME}' is not a processor module",
            "TID", tid, "NAME", std::string(terminusName));
        return PLDM_ERROR_INVALID_DATA;
    }

    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        auto method =
            bus.new_method_call(nvidiaInfoService, nvidiaInfoObjectPath,
                                nvidiaInfoInterface, "CreateInfo");
        method.append(*moduleIndex, std::string(*payload));
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to send inventory of terminus {TID}, error - {ERROR}",
            "TID", tid, "ERROR", e);
        return PLDM_ERROR;
    }

    lg2::info(
        "Sent {SIZE} bytes of inventory from terminus {TID} for processor module {MODULE}",
        "SIZE", payload->size(), "TID", tid, "MODULE", *moduleIndex);

    return PLDM_SUCCESS;
}

} // namespace oem_nvidia
} // namespace pldm
