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

Response invokeHandler(uint8_t pldmType, uint8_t pldmCommand,
                       const pldm_msg* request, size_t payloadLength)
{
    using namespace internal;
    Response response;
    if (!(typeHandlers.end() == typeHandlers.find(pldmType)))
    {
        if (!((typeHandlers.at(pldmType)).end() ==
              (typeHandlers.at(pldmType)).find(pldmCommand)))
        {
            response = typeHandlers.at(pldmType).at(pldmCommand)(request,
                                                                 payloadLength);
            if (response.empty())
            {
                log<level::ERR>("Encountered invalid response");
            }
        }
        else
        {
            log<level::ERR>("Unsupported PLDM command",
                            entry("command=0x%02x", pldmCommand));
        }
    }
    else
    {
        log<level::ERR>("Unsupported PLDM TYPE",
                        entry("type=0x%02x", pldmType));
    }

    return response;
}

} // namespace responder
} // namespace pldm
