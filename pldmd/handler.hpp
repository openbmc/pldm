#pragma once

#include "libpldm/base.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

namespace pldm
{

using Command = uint8_t;

namespace responder
{

using Response = std::vector<uint8_t>;
class CmdHandler;
using HandlerFunc =
    std::function<Response(const pldm_msg* request, size_t reqMsgLen)>;

class CmdHandler
{
  public:
    /** @brief Invoke a PLDM command handler
     *
     *  @param[in] pldmCommand - PLDM command code
     *  @param[in] request - PLDM request message
     *  @param[in] reqMsgLen - PLDM request message size
     *  @param[in] isRespMsg - whether it is a request or response
     *  @return PLDM response message
     */
    Response handle(Command pldmCommand, const pldm_msg* request,
                    size_t reqMsgLen, bool isRespMsg)
    {
        if (isRespMsg)
        {
            return respHandlers.at(pldmCommand)(request, reqMsgLen);
        }
        return handlers.at(pldmCommand)(request, reqMsgLen);
    }

    /** @brief Create a response message containing only cc
     *
     *  @param[in] request - PLDM request message
     *  @param[in] cc - Completion Code
     *  @return PLDM response message
     */
    static Response ccOnlyResponse(const pldm_msg* request, uint8_t cc)
    {
        Response response(sizeof(pldm_msg), 0);
        auto ptr = reinterpret_cast<pldm_msg*>(response.data());
        auto rc =
            encode_cc_only_resp(request->hdr.instance_id, request->hdr.type,
                                request->hdr.command, cc, ptr);
        assert(rc == PLDM_SUCCESS);
        return response;
    }

  protected:
    /** @brief maps of PLDM command code to request handler or response handler-
     *         to be populated by derived
     *         classes.
     */
    std::map<Command, HandlerFunc> handlers;
    std::map<Command, HandlerFunc> respHandlers;
};

} // namespace responder
} // namespace pldm
