#pragma once

#include "handler.hpp"

#include <libpldm/base.h>
#include <libpldm/pldm.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <map>
#include <memory>

namespace pldm
{

using Type = uint8_t;

namespace responder
{

/** @struct RequestKey
 *
 *  RequestKey uniquely identifies the PLDM request message to match it with the
 *  response and a combination of MCTP endpoint ID, PLDM instance ID, PLDM type
 *  and PLDM command is the key.
 */
struct RequestKey
{
    mctp_eid_t eid;     //!< MCTP endpoint ID
    uint8_t instanceId; //!< PLDM instance ID
    uint8_t type;       //!< PLDM type
    uint8_t command;    //!< PLDM command

    bool operator==(const RequestKey& e) const
    {
        return ((eid == e.eid) && (instanceId == e.instanceId) &&
                (type == e.type) && (command == e.command));
    }
};

struct RequestCache
{
    RequestKey key;
    Response response;
    std::unique_ptr<sdbusplus::Timer> timer;
};

class Invoker
{
  public:
    Invoker(sdeventplus::Event& event) : event(event) {}
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
        if (!request)
        {
            lg2::error(
                "Failed to handle request: Invalid request message pointer");
            return CmdHandler::ccOnlyResponse(request, PLDM_ERROR);
        }

        RequestKey reqKey{tid, request->hdr.instance_id, pldmType, pldmCommand};
        if (requestCaches.contains(tid) && (requestCaches[tid].key == reqKey))
        {
            // This is a retried request successfully acted upon, re-use the
            // original response.
            lg2::info(
                "Detected a retried request that was successfully acted upon "
                "with match values:\nEID {EID}, PLDM Type {TYPE}, "
                "PLDM Command Code {CMD}, Instance ID {ID}."
                "\nResending response...",
                "EID", reqKey.eid, "TYPE", reqKey.type, "CMD", reqKey.command,
                "ID", reqKey.instanceId);
            return requestCaches[tid].response;
        }

        if (requestCaches.contains(tid))
        {
            requestCaches.erase(tid);
        }

        if (removeEvents.contains(tid))
        {
            removeEvents.erase(tid);
        }

        if (handlers.find(pldmType) == handlers.end())
        {
            return CmdHandler::ccOnlyResponse(request,
                                              PLDM_ERROR_INVALID_PLDM_TYPE);
        }

        auto response =
            handlers.at(pldmType)->handle(tid, pldmCommand, request, reqMsgLen);

        if (response.size() < sizeof(pldm_msg))
        {
            // Don't cache a request that has an invalid response length
            return response;
        }

        auto respMsg = reinterpret_cast<const pldm_msg*>(response.data());
        if (respMsg->payload[0] != PLDM_SUCCESS)
        {
            // Don't cache a request that is failed to be handled
            return response;
        }

        // Timer to erase a request that has an expired instance ID from cache
        auto timer = std::make_unique<sdbusplus::Timer>(
            event.get(), [this, tid, reqKey]() {
                this->removeEvents.emplace(
                    tid, std::make_unique<sdeventplus::source::Defer>(
                             this->event, std::bind(&Invoker::removeCache, this,
                                                    tid, reqKey)));
            });

        try
        {
            std::chrono::seconds instanceIdExpiryInterval =
                std::chrono::seconds(5);
            timer->start(instanceIdExpiryInterval, false);
        }
        catch (const std::runtime_error& e)
        {
            lg2::error(
                "Failed to start the Instance ID expiry timer with error {ERROR}.",
                "ERROR", e);
            return response;
        }

        RequestCache cache{reqKey, response, std::move(timer)};
        requestCaches.emplace(tid, std::move(cache));

        return response;
    }

    void removeCache(mctp_eid_t tid, const RequestKey& reqKey)
    {
        if (removeEvents.contains(tid))
        {
            removeEvents.erase(tid);
        }
        if (requestCaches.contains(tid) && (requestCaches[tid].key == reqKey))
        {
            auto& sharedTimer = requestCaches[tid].timer;
            if (!sharedTimer)
            {
                lg2::error("Timer instance is null");
                return;
            }

            auto rc = sharedTimer->stop();
            if (rc)
            {
                lg2::error(
                    "Failed to stop the Instance ID expiry timer with error {RC}.",
                    "RC", rc);
            }
            requestCaches.erase(tid);
        }
    }

  private:
    sdeventplus::Event& event;
    std::map<Type, std::unique_ptr<CmdHandler>> handlers;
    /** @brief Container to store cached info of the last request from each
     * enpoint
     */
    std::map<mctp_eid_t, RequestCache> requestCaches;
    /** @brief Event sources to defer erasing cached request from each enpoint
     */
    std::map<mctp_eid_t, std::unique_ptr<sdeventplus::source::Defer>>
        removeEvents;
};

} // namespace responder
} // namespace pldm
