#pragma once

#include <libpldm/firmware_update.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

namespace pldm::fw_update
{

class ComponentActivationMethods
{
  public:
    static constexpr uint16_t automatic = 1U << 0;
    static constexpr uint16_t selfContained = 1U << 1;
    static constexpr uint16_t mediumReset = 1U << 2;
    static constexpr uint16_t systemReboot = 1U << 3;
    static constexpr uint16_t dcPowerCycle = 1U << 4;
    static constexpr uint16_t acPowerCycle = 1U << 5;
    static constexpr uint16_t commonMethods = 0x003f;

    void recordApplied(size_t component, uint16_t requested, uint16_t supported,
                       uint8_t result, uint16_t modification)
    {
        if (result != PLDM_FWUP_APPLY_SUCCESS &&
            result != PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD)
        {
            return;
        }
        if (result == PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD)
        {
            supported = modification;
        }
        applied.insert_or_assign(component, Methods{requested, supported});
    }

    bool requestSelfContained() const
    {
        bool needed = false;
        for (const auto& [component, methods] : applied)
        {
            const auto eligible = methods.eligible();
            if ((eligible & automatic) != 0)
            {
                continue;
            }
            if ((methods.supported & selfContained) != 0)
            {
                if ((eligible & selfContained) == 0)
                {
                    return false;
                }
                needed = true;
            }
        }
        return needed;
    }

    std::string pendingMethods(bool selfContainedRequested) const
    {
        std::string pending;
        for (const auto& [component, methods] : applied)
        {
            const auto eligible = methods.eligible();
            if ((eligible & automatic) != 0 ||
                (selfContainedRequested && (eligible & selfContained) != 0))
            {
                continue;
            }
            const char* action = "UnknownActivationMethod";
            for (const auto& [mask, name] : manualMethods)
            {
                if ((eligible & mask) != 0)
                {
                    action = name;
                    break;
                }
            }
            if (!pending.empty())
            {
                pending += "; ";
            }
            pending += "Component " + std::to_string(component) + ": " + action;
        }
        return pending;
    }

  private:
    struct Methods
    {
        uint16_t requested;
        uint16_t supported;

        uint16_t eligible() const
        {
            return requested & supported & commonMethods;
        }
    };

    static constexpr std::array<std::pair<uint16_t, const char*>, 4>
        manualMethods{{{mediumReset, "MediumSpecificReset"},
                       {systemReboot, "SystemReboot"},
                       {dcPowerCycle, "DCPowerCycle"},
                       {acPowerCycle, "ACPowerCycle"}}};
    std::map<size_t, Methods> applied;
};

} // namespace pldm::fw_update