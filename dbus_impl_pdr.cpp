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
    std::cerr << "inside the method" << std::endl;
    uint8_t pdrType = PLDM_STATE_EFFECTER_PDR;
    uint8_t* outData = nullptr;
    uint32_t size{};
    bool found = false;
    std::vector<uint8_t> effecter_pdr;
    const pldm_pdr_record* record{};
    do
    {
        record = pldm_pdr_find_record_by_type(repo, pdrType, record, &outData,
                                              &size);
        pldm_state_effecter_pdr* pdr =
            reinterpret_cast<pldm_state_effecter_pdr*>(outData);
        uint8_t compositeEffecterCount = pdr->composite_effecter_count;
        state_effecter_possible_states* possibleStates =
            reinterpret_cast<state_effecter_possible_states*>(
                pdr->possible_states);
        uint16_t setId = possibleStates->state_set_id;
        uint16_t possibleStateSize = possibleStates->possible_states_size;
        for (uint8_t effecters = 0x01; effecters <= compositeEffecterCount;
             effecters++)
        {
            if (possibleStateSize != 0x00)
            {
                state_effecter_possible_states* states =
                    reinterpret_cast<state_effecter_possible_states*>(
                        pdr->possible_states + possibleStateSize +
                        sizeof(setId) + sizeof(possibleStateSize));

                if (record && pdr->container_id == containerId &&
                    pdr->entity_type == entityType &&
                    pdr->entity_instance == entityInstance &&
                    states->state_set_id == stateSetId)
                {
                    effecter_pdr.resize(size);
                    memcpy(effecter_pdr.data(), outData, effecter_pdr.size());
                    found = true;
                    break;
                }
            }
        }

        if (found)
        {
            break;
        }
    } while (record);
    free(outData);

    return effecter_pdr;
}
} // namespace dbus_api
} // namespace pldm
