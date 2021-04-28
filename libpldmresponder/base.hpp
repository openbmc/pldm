#pragma once

#include "libpldm/base.h"

#include "libpldmresponder/platform.hpp"
#include "pldmd/handler.hpp"

#include <stdint.h>

#include <sdeventplus/source/event.hpp>

#include <vector>

using namespace pldm::dbus_api;
using namespace pldm::responder;

namespace pldm
{
namespace responder
{
namespace base
{

class Handler : public CmdHandler
{
  public:
    Handler(pldm::responder::oem_platform::Handler* oemPlatformHandler,
            int mctp_fd, uint8_t mctp_eid, Requester& requester,
            sdeventplus::Event& event) :
        oemPlatformHandler(oemPlatformHandler),
        mctp_fd(mctp_fd), mctp_eid(mctp_eid), requester(requester), event(event)
    {
        handlers.emplace(PLDM_GET_PLDM_TYPES,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->getPLDMTypes(request, payloadLength);
                         });
        handlers.emplace(PLDM_GET_PLDM_COMMANDS, [this](const pldm_msg* request,
                                                        size_t payloadLength) {
            return this->getPLDMCommands(request, payloadLength);
        });
        handlers.emplace(PLDM_GET_PLDM_VERSION, [this](const pldm_msg* request,
                                                       size_t payloadLength) {
            return this->getPLDMVersion(request, payloadLength);
        });
        handlers.emplace(PLDM_GET_TID,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->getTID(request, payloadLength);
                         });
    }

    /** @brief Handler for getPLDMTypes
     *
     *  @param[in] request - Request message payload
     *  @param[in] payload_length - Request message payload length
     *  @param[return] Response - PLDM Response message
     */
    Response getPLDMTypes(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for getPLDMCommands
     *
     *  @param[in] request - Request message payload
     *  @param[in] payload_length - Request message payload length
     *  @param[return] Response - PLDM Response message
     */
    Response getPLDMCommands(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for getPLDMCommands
     *
     *  @param[in] request - Request message payload
     *  @param[in] payload_length - Request message payload length
     *  @param[return] Response - PLDM Response message
     */
    Response getPLDMVersion(const pldm_msg* request, size_t payloadLength);

    /** @brief _processSetEventReceiver does the actual work that needs
     *  to be carried out for setEventReceiver command. This is deferred
     *  after sending response for getTID command to the host
     *  @param[in] source - sdeventplus event source
     */
    void _processSetEventReceiver(sdeventplus::source::EventBase& source);

    /** @brief Handler for getTID
     *
     *  @param[in] request - Request message payload
     *  @param[in] payload_length - Request message payload length
     *  @param[return] Response - PLDM Response message
     */
    Response getTID(const pldm_msg* request, size_t payloadLength);

    /** @OEM platform handler */
    pldm::responder::oem_platform::Handler* oemPlatformHandler;

    /** @brief fd of MCTP communications socket */
    int mctp_fd;

    /** @brief MCTP EID of host firmware */
    uint8_t mctp_eid;

    /** @brief reference to Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    Requester& requester;

  private:
    /** @brief reference of main event loop of pldmd, primarily used to schedule
     *  work
     */
    sdeventplus::Event& event;

    /** @brief sdeventplus event source */
    std::unique_ptr<sdeventplus::source::Defer> survEvent;
};

} // namespace base
} // namespace responder
} // namespace pldm
