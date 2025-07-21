#pragma once

#define PLDM_OEM_EVENT_CLASS_0xFB 0xFB

#include "oem/meta/event/types.hpp"
#include "platform-mc/manager.hpp"

#include <libpldm/base.h>

#include <span>

namespace pldm::oem_meta
{

using eventMsg = std::span<const uint8_t>;

class OemEventManager
{
  public:
    OemEventManager() = default;
    OemEventManager(const OemEventManager&) = delete;
    OemEventManager(OemEventManager&&) = delete;
    OemEventManager& operator=(const OemEventManager&) = delete;
    OemEventManager& operator=(OemEventManager&&) = delete;
    virtual ~OemEventManager() = default;

    /** @brief Handle a PLDM OEM event.
     *
     *  @param[in] request - The PLDM request message.
     *  @param[in] payloadLength - The length of the PLDM request payload.
     *  @param[in] formatVersion - The format version of the event message.
     *  @param[in] tid - The PLDM terminal ID.
     *  @param[in] eventDataOffset - The offset of the event data in the
     * request.
     *
     *  @return 0 on success, error code on failure.
     */
    int handleOemEvent(const pldm_msg* request, size_t payloadLength,
                       uint8_t /* formatVersion */, pldm_tid_t tid,
                       size_t eventDataOffset) const;

  private:
    // @brief The event message length with timestamp.
    size_t eventMsgLengthWithTimestamp = 20;

    /** @brief Process an OEM Meta event.
     *
     *  @param[in] tid - The PLDM terminal ID.
     *  @param[in] eventData - A pointer to the event data.
     *  @param[in] eventDataSize - The size of the event data.
     *
     *  @return 0 on success, error code on failure.
     */
    int processOemMetaEvent(pldm_tid_t tid, const uint8_t* eventData,
                            size_t eventDataSize) const;

    /** @brief Handle a system event.
     *
     *  @param[in] eventData - A pointer to the event data.
     *  @param[out] message - The resulting event message.
     */
    void handleSystemEvent(const eventMsg& eventData,
                           std::string& message) const;

    /** @brief Handle a unified BIOS event.
     *
     *  @param[in] tid - The PLDM terminal ID.
     *  @param[in] eventData - A pointer to the event data.
     *  @param[out] message - The resulting event message.
     */
    void handleUnifiedBIOSEvent(pldm_tid_t tid, const eventMsg& eventData,
                                std::string& message) const;

    /** @brief Handle a memory error event.
     *
     *  @param[in] eventData - A pointer to the event data.
     *  @param[out] message - The resulting event message.
     *  @param[in] dimmInfo - Information about the DIMM.
     *  @param[in] generalInfo - General information about the event.
     */
    void handleMemoryError(const eventMsg& eventData, std::string& message,
                           const DimmInfo& dimmInfo, uint8_t generalInfo) const;

    /** @brief Handle a system POST event.
     *
     *  @param[in] eventData - A pointer to the event data.
     *  @param[out] message - The resulting event message.
     *  @param[in] generalInfo - General information about the event.
     */
    void handleSystemPostEvent(const eventMsg& eventData, std::string& message,
                               uint8_t generalInfo) const;

    /** @brief Get the DIMM device name.
     *
     *  @param[in] eventData - A pointer to the event data.
     *  @param[in] bdfIdx - The index of the BDF (Bus/Device/Function).
     *
     *  @return The DIMM device name.
     */
    std::string getDimmDeviceName(const eventMsg& eventData, int bdfIdx) const
    {
        std::string dimmDevName = "";
        for (size_t i = 0; i < cxlBdfMap.size(); i++)
        {
            if ((eventData[bdfIdx] == cxlBdfMap[i].Bus) &&
                ((eventData[bdfIdx + 1] >> 3) == cxlBdfMap[i].Dev) &&
                ((eventData[bdfIdx + 1] & 0x7) == cxlBdfMap[i].Fun))
            {
                dimmDevName = std::string(cxlBdfMap[i].Name) + "_";
                break;
            }
        }
        return dimmDevName;
    };

    /** @brief Convert a byte to a hex string.
     *
     *  @param[in] value - The byte value to convert.
     *  @param[in] len - The length of the resulting string.
     *
     *  @return The hex string representation of the byte.
     */
    inline auto to_hex_string(uint8_t value, size_t len = 2) const
    {
        return std::format("{:02x}", value, len);
    }

    /** @brief Convert CPU, channel, and slot to a DIMM string.
     *
     *  @param[in] cpu - The CPU number.
     *  @param[in] channel - The channel number.
     *  @param[in] slot - The slot number.
     *  @param[out] str - The resulting DIMM string.
     */
    void convertToDimmString(uint8_t cpu, uint8_t channel, uint8_t slot,
                             std::string& str) const
    {
        constexpr char label[] = {'A', 'C', 'B', 'D'};
        constexpr size_t labelSize = sizeof(label);

        size_t idx = cpu * 2 + slot;
        if (idx < labelSize)
        {
            str = label[idx] + std::to_string(channel);
        }
        else
        {
            str = "NA";
        }
    }

    /** @brief Get the common DIMM location string.
     *
     *  @param[in] dimmInfo - Information about the DIMM.
     *  @param[out] dimmLocation - The resulting DIMM location string.
     *  @param[out] dimm - The resulting DIMM string.
     */
    void getCommonDimmLocation(const DimmInfo& dimmInfo,
                               std::string& dimmLocation,
                               std::string& dimm) const
    {
        std::string sled_str = std::to_string(dimmInfo.sled);
        std::string socket_str = std::to_string(dimmInfo.socket);

        // Check Channel and Slot
        if (dimmInfo.channel == 0xFF && dimmInfo.slot == 0xFF)
        {
            dimm = "unknown";
            dimmLocation = "DIMM Slot Location: Sled " + sled_str + "/Socket " +
                           socket_str +
                           ", Channel unknown, Slot unknown, DIMM unknown";
        }
        else
        {
            uint8_t channel = dimmInfo.channel & 0x0F;
            uint8_t slot = dimmInfo.slot & 0x07;
            convertToDimmString(dimmInfo.socket, channel, slot, dimm);

            std::string channel_str = std::to_string(channel);
            std::string slot_str = std::to_string(slot);

            dimmLocation = "DIMM Slot Location: Sled " + sled_str + "/Socket " +
                           socket_str + ", Channel " + channel_str + ", Slot " +
                           slot_str + ", DIMM " + dimm;
        }
    }
};

} // namespace pldm::oem_meta
