#pragma once

#define PLDM_OEM_EVENT_CLASS_0xFB 0xFB

#include "oem/meta/event/types.hpp"
#include "platform-mc/manager.hpp"

#include <libpldm/base.h>

namespace pldm::oem_meta
{

class OemEventManager
{
  public:
    OemEventManager() = default;
    OemEventManager(const OemEventManager&) = delete;
    OemEventManager(OemEventManager&&) = delete;
    OemEventManager& operator=(const OemEventManager&) = delete;
    OemEventManager& operator=(OemEventManager&&) = delete;
    virtual ~OemEventManager() = default;

    int handleOemEvent(const pldm_msg* request, size_t payloadLength,
                       uint8_t /* formatVersion */, pldm_tid_t tid,
                       size_t eventDataOffset) const;

  private:
    int processOemMetaEvent(pldm_tid_t tid, const uint8_t* eventData, size_t eventDataSize)
        const;

    void handleSystemEvent(const uint8_t*, std::string&) const;

    void handleUnifiedBIOSEvent(const uint8_t*, std::string&) const;

    void handleMemoryError(const uint8_t*, std::string&, const DimmInfo&,
                           uint8_t generalInfo) const;

    void handleSystemPostEvent(const uint8_t*, std::string&,
                               uint8_t generalInfo) const;

    inline auto to_hex_string(uint8_t value, size_t len = 2) const
    {
        return std::format("{:02x}", value, len);
    }

    void covertToDimmString(uint8_t cpu, uint8_t channel, uint8_t slot,
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
            covertToDimmString(dimmInfo.socket, channel, slot, dimm);

            std::string channel_str = std::to_string(channel);
            std::string slot_str = std::to_string(slot);

            dimmLocation = "DIMM Slot Location: Sled " + sled_str + "/Socket " +
                           socket_str + ", Channel " + channel_str + ", Slot " +
                           slot_str + ", DIMM " + dimm;
        }
    }
};

} // namespace pldm::oem_meta
