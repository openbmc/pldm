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

std::vector<std::vector<uint8_t>> Pdr::findStateEffecterPDR(uint8_t /*tid*/,
                                                            uint16_t entityID,
                                                            uint16_t stateSetId)
{
    uint8_t* outData = nullptr;
    uint32_t size{};
    const pldm_pdr_record* record{};
    std::vector<std::vector<uint8_t>> pdrs;
    try
    {
        do
        {
            record = pldm_pdr_find_record_by_type(
                pdrRepo, PLDM_STATE_EFFECTER_PDR, record, &outData, &size);
            if (record)
            {
                auto pdr = reinterpret_cast<pldm_state_effecter_pdr*>(outData);
                auto compositeEffecterCount = pdr->composite_effecter_count;
                for (auto effecters = 0x01; effecters <= compositeEffecterCount;
                     effecters++)
                {
                    auto possibleStates =
                        reinterpret_cast<state_effecter_possible_states*>(
                            pdr->possible_states);
                    auto setId = possibleStates->state_set_id;
                    auto possibleStateSize =
                        possibleStates->possible_states_size;


                        if (pdr->entity_type == entityID &&
                            setId == stateSetId)
                        {
                            std::vector<uint8_t> effecter_pdr(&outData[0],
                                                              &outData[size]);
                            pdrs.emplace_back(std::move(effecter_pdr));

                            break;

                        }

                    possibleStates += possibleStateSize + sizeof(setId) +
                                      sizeof(possibleStateSize);
                }
            }

        } while (record);
    }
    catch (const std::exception& e)
    {
        throw ResourceNotFound();
    }
    if( pdrs.empty())
    {
       throw ResourceNotFound();
    }

    return pdrs;
}
} // namespace dbus_api
} // namespace pldm
