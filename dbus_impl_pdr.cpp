#include "dbus_impl_pdr.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <iostream>

#include "libpldm/pdr.h"
#include "libpldm/pldm_types.h"

using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace pldm
{
namespace dbus_api
{

std::vector<uint8_t> Pdr::findStateEffecterPDR(uint8_t /*tid*/,
                                               uint16_t containerId,
                                               uint16_t entityType,
                                               uint16_t entityInstance,
                                               uint16_t stateSetId)
{
    uint8_t* outData = nullptr;
    uint32_t size{};
    const pldm_pdr_record* record{};
    do
    {
        record = pldm_pdr_find_record_by_type(pdrRepo, PLDM_STATE_EFFECTER_PDR,
                                              record, &outData, &size);
        if (record)
        {
            auto pdr = reinterpret_cast<pldm_state_effecter_pdr*>(outData);
            auto compositeEffecterCount = pdr->composite_effecter_count;
            for (auto effecters = 0x01; effecters <= compositeEffecterCount;
                 effecters++)
            {
                state_effecter_possible_states* possibleStates =
                    reinterpret_cast<state_effecter_possible_states*>(
                        pdr->possible_states);
                auto setId = possibleStates->state_set_id;
                auto possibleStateSize = possibleStates->possible_states_size;

                if (possibleStateSize != 0x00)
                {

                    if (pdr->container_id == containerId &&
                        pdr->entity_type == entityType &&
                        pdr->entity_instance == entityInstance &&
                        setId == stateSetId)
                    {
                        std::vector<uint8_t> effecter_pdr(&outData[0],
                                                          &outData[size]);
                        free(outData);
                        return effecter_pdr;
                    }
                }

                possibleStates += possibleStateSize + sizeof(setId) +
                                  sizeof(possibleStateSize);
            }
        }
        else
        {
            free(outData);
            continue;
        }

    } while (record);

    return {};
}
} // namespace dbus_api
} // namespace pldm
