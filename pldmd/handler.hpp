#pragma once

#include <libpldm/base.h>

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
using HandlerFunc = std::function<Response(
    pldm_tid_t tid, const pldm_msg* request, size_t reqMsgLen)>;

class CmdHandler
{
  public:
    virtual ~CmdHandler() = default;

    /** @brief Invoke a PLDM command handler
     *
     *  @param[in] tid - PLDM request TID
     *  @param[in] pldmCommand - PLDM command code
     *  @param[in] request - PLDM request message
     *  @param[in] reqMsgLen - PLDM request message size
     *  @return PLDM response message
     */
    Response handle(pldm_tid_t tid, Command pldmCommand,
                    const pldm_msg* request, size_t reqMsgLen)
    {
        return handlers.at(pldmCommand)(tid, request, reqMsgLen);
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
        auto ptr = new (response.data()) pldm_msg;
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
