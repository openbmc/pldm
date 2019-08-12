#include "registration.hpp"

#include <map>
#include <phosphor-logging/log.hpp>

using namespace phosphor::logging;

namespace pldm
{

namespace responder
{

namespace internal
{
using Command = uint8_t;
using CommandHandler = std::map<Command, Handler>;
using Type = uint8_t;
std::map<Type, CommandHandler> typeHandlers;
} // namespace internal

void registerHandler(uint8_t pldmType, uint8_t pldmCommand, Handler&& handler)
{
    using namespace internal;
    CommandHandler& ch = typeHandlers[pldmType];
    ch.emplace(pldmCommand, std::move(handler));
}

Response invokeHandler(const Interfaces& intfs, uint8_t pldmType,
                       uint8_t pldmCommand, const Request& request,
                       size_t payloadLength)
{
    using namespace internal;
    auto response = typeHandlers.at(pldmType).at(pldmCommand)(intfs, request,
                                                              payloadLength);
    ;
    return response;
}

} // namespace responder
} // namespace pldm
