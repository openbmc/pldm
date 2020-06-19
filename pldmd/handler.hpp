#pragma once

#include "libpldm/base.h"

#include <cassert>
#include <functional>
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
     *  @return PLDM response message
     */
    Response handle(Command pldmCommand, const pldm_msg* request,
                    size_t reqMsgLen)
    {
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
    /** @brief map of PLDM command code to handler - to be populated by derived
     *         classes.
     */
    std::map<Command, HandlerFunc> handlers;
};

} // namespace responder
} // namespace pldm
