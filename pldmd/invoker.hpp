#pragma once

#include "handler.hpp"

#include <libpldm/base.h>

#include <map>
#include <memory>

namespace pldm
{

using Type = uint8_t;

namespace responder
{

class Invoker
{
  public:
    /** @brief Register a handler for a PLDM Type
     *
     *  @param[in] pldmType - PLDM type code
     *  @param[in] handler - PLDM Type handler
     */
    void registerHandler(Type pldmType, std::unique_ptr<CmdHandler> handler)
    {
        handlers.emplace(pldmType, std::move(handler));
    }

    /** @brief Invoke a PLDM command handler
     *
     *  @param[in] tid - PLDM request TID
     *  @param[in] pldmType - PLDM type code
     *  @param[in] pldmCommand - PLDM command code
     *  @param[in] request - PLDM request message
     *  @param[in] reqMsgLen - PLDM request message size
     *  @return PLDM response message
     */
    Response handle(pldm_tid_t tid, Type pldmType, Command pldmCommand,
                    const pldm_msg* request, size_t reqMsgLen)
    {
        return handlers.at(pldmType)->handle(tid, pldmCommand, request,
                                             reqMsgLen);
    }

  private:
    std::map<Type, std::unique_ptr<CmdHandler>> handlers;
};

} // namespace responder
} // namespace pldm
