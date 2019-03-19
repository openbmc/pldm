#include "libpldm/platform.h"

#include "platform.hpp"

#include <array>

namespace pldm
{
namespace responder
{

void setStateEffecterStates(const pldm_msg_payload* request, pldm_msg* response)
{
    uint16_t effecterId;
    uint8_t compEffecterCnt;
    std::array<state_field_set_state_effecter_state, 8> stateField{};

    if (request->payload_length <
        (sizeof(effecterId) + sizeof(compEffecterCnt) +
         sizeof(state_field_set_state_effecter_state)))
    {
        encode_set_state_effecter_states_resp(0, PLDM_ERROR_INVALID_LENGTH,
                                              response);
        return;
    }

    decode_set_state_effecter_states_req(request, &effecterId, &compEffecterCnt,
                                         stateField.data());

    // TODO effecter router to be added
    encode_set_state_effecter_states_resp(0, PLDM_SUCCESS, response);
}

} // namespace responder

} // namespace pldm
