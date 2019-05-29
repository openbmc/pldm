#include "registration.hpp"

#include <map>

namespace pldm
{

namespace responder
{

namespace internal
{
using CommandHandler = std::map<uint8_t, Handler>;
std::map<uint8_t, CommandHandler> typeHandlers;
} // namespace internal

void registerHandler(uint8_t pldmType, uint8_t pldmCommand, Handler&& handler)
{
    using namespace internal;
    CommandHandler& ch = typeHandlers[pldmType];
    ch.emplace(pldmCommand, std::move(handler));
}

void invokeHandler(uint8_t pldmType, uint8_t pldmCommand,
                   const pldm_msg_payload* request, pldm_msg* response)
{
    using namespace internal;
    typeHandlers.at(pldmType).at(pldmCommand)(request, response);
}

} // namespace responder
} // namespace pldm
