#pragma once

// #include "config.h"

// #include "handler.hpp"
// #include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/platform_11.hpp"
#include "libpldmresponder/platform_9.hpp"
// #include "libpldmresponder/utils.hpp"

// #include <stdint.h>

// #include <map>

// #include "libpldm/platform.h"
// #include "libpldm/states.h"

namespace pldm
{
namespace responder
{
namespace platform
{

class Handler : public CmdHandler
{
  public:
    Handler()
    {
        handlers.emplace(PLDM_GET_PDR,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->getPDR(request, payloadLength);
                         });
        handlers.emplace(PLDM_SET_NUMERIC_EFFECTER_VALUE,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->setNumericEffecterValue(
                                 request, payloadLength);
                         });
        handlers.emplace(PLDM_SET_STATE_EFFECTER_STATES,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->setStateEffecterStates(request,
                                                                 payloadLength);
                         });
    }

    /** @brief Handler for GetPDR
     *
     *  @param[in] request - Request message payload
     *  @param[in] payloadLength - Request payload length
     *  @param[out] Response - Response message written here
     */
    Response getPDR(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for setNumericEffecterValue
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @return Response - PLDM Response message
     */
    Response setNumericEffecterValue(const pldm_msg* request,
                                     size_t payloadLength);

    /** @brief Handler for setStateEffecterStates
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @return Response - PLDM Response message
     */
    Response setStateEffecterStates(const pldm_msg* request,
                                    size_t payloadLength);
};

} // namespace platform
} // namespace responder
} // namespace pldm
