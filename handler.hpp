#pragma once

#include <functional>
#include <map>

#include "libpldm/base.h"

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

  protected:
    /** @brief map of PLDM command code to handler - to be populated by derived
     *         classes.
     */
    std::map<Command, HandlerFunc> handlers;
};

} // namespace responder
} // namespace pldm
