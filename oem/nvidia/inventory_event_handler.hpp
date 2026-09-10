#pragma once

#include <libpldm/base.h>

#include <sdbusplus/slot.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>

namespace pldm
{
namespace oem_nvidia
{

/** @brief Extract the JSON document from an inventory event
 *
 *  The event data starts with a four byte header holding the format version,
 *  the format type and the payload size as a little endian uint16, followed by
 *  the UTF-8 JSON document.
 *
 *  @param[in] eventData - event data including the header
 *  @param[in] eventDataSize - size of the event data
 *
 *  @return the JSON document, or std::nullopt when the event is malformed
 */
std::optional<std::string_view> parseInventoryEventPayload(
    const uint8_t* eventData, size_t eventDataSize);

/** @brief Derive the processor module index from a terminus name
 *
 *  @param[in] terminusName - name of the terminus the event came from
 *
 *  @return the index, or std::nullopt when the name is not a processor module
 */
std::optional<int32_t> processorModuleIndex(std::string_view terminusName);

/**
 * @class InventoryEventHandler
 *
 * @brief Forwards inventory documents to the hosting service
 */
class InventoryEventHandler
{
  public:
    /** @brief Forward the inventory document of one event to the hosting
     *         service
     *
     *  The D-Bus call is asynchronous, so the returned completion code only
     *  covers the validation of the event; a failed call is logged when its
     *  reply arrives.
     *
     *  @param[in] tid - TID the event came from
     *  @param[in] terminusName - name of the terminus, empty when unknown
     *  @param[in] eventData - event data including the header
     *  @param[in] eventDataSize - size of the event data
     *
     *  @return PLDM completion code
     */
    int handleEvent(pldm_tid_t tid, std::string_view terminusName,
                    const uint8_t* eventData, size_t eventDataSize);

  private:
    /** @brief Pending D-Bus calls, one per terminus */
    std::map<pldm_tid_t, sdbusplus::slot_t> pendingCalls;
};

} // namespace oem_nvidia
} // namespace pldm
