#pragma once

#define PLDM_OEM_EVENT_CLASS_0xFB 0xFB

#include "common/types.hpp"
#include "requester/configuration_discovery_handler.hpp"

namespace pldm
{
namespace oem_meta
{

constexpr auto MetaIANA = "0015A000";

class OemEventManager
{
  public:
    OemEventManager() = delete;
    OemEventManager(const OemEventManager&) = delete;
    OemEventManager(OemEventManager&&) = delete;
    OemEventManager& operator=(const OemEventManager&) = delete;
    OemEventManager& operator=(OemEventManager&&) = delete;
    virtual ~OemEventManager() = default;

    explicit OemEventManager(
        std::shared_ptr<pldm::requester::ConfigurationDiscoveryHandler>
            configurationDiscovery,
        pldm::platform_mc::Manager* platformManager) :
        platformManager(platformManager),
        configurationDiscovery(configurationDiscovery)
    {}

    int handleOemEvent(const pldm_msg* request, size_t payloadLength,
                       uint8_t /* formatVersion */, uint8_t tid,
                       size_t eventDataOffset);

  private:
    void covertToDimmString(uint8_t cpu, uint8_t channel, uint8_t slot,
                            std::string& str) const;

    void getCommonDimmLocation(const dimm_info& dimmInfo,
                               std::string& dimmLocation,
                               std::string& dimm) const;

    inline auto to_hex_string(uint8_t value) const;
    {
        return std::format("{:02x}", value);
    }

    inline bool EventManager::checkMetaIana(
        pldm_tid_t tid,
        const std::map<std::string, MctpEndpoint>& configurations)
    {
        for (const auto& [configDbusPath, mctpEndpoint] : configurations)
        {
            if (mctpEndpoint.EndpointId == tid)
            {
                if (mctpEndpoint.iana.has_value() &&
                    mctpEndpoint.iana.value() == MetaIANA)
                {
                    return true;
                }
            }
        }
        return false;
    }

    int processOemMetaEvent(
        pldm_tid_t tid, const uint8_t* eventData, size_t eventDataSize,
        const std::map<std::string, MctpEndpoint>& configurations) const;

    std::string getSlotNumberString(
        pldm_tid_t tid,
        const std::map<std::string, MctpEndpoint>& configurations) const;

    std::shared_ptr<pldm::requester::ConfigurationDiscoveryHandler>
        configurationDiscovery{};

    pldm::platform_mc::Manager* platformManager = nullptr;
}

} // namespace oem_meta
} // namespace pldm
