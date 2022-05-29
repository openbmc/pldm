#pragma once

#include "config.h"

#include "libpldm/base.h"
#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "request.hpp"

#include <sys/socket.h>

#include <function2/function2.hpp>
#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <cassert>
#include <chrono>
#include <coroutine>
#include <memory>
#include <tuple>
#include <unordered_map>

namespace pldm
{

namespace requester
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

/** @struct RequestKeyHasher
 *
 *  This is a simple hash function, since the instance ID generator API
 *  generates unique instance IDs for MCTP endpoint ID.
 */
struct RequestKeyHasher
{
    std::size_t operator()(const RequestKey& key) const
    {
        return (key.eid << 24 | key.instanceId << 16 | key.type << 8 |
                key.command);
    }
};

using ResponseHandler = fu2::unique_function<void(
    mctp_eid_t eid, const pldm_msg* response, size_t respMsgLen)>;

/** @class Handler
 *
 *  This class handles the lifecycle of the PLDM request message based on the
 *  instance ID expiration interval, number of request retries and the timeout
 *  waiting for a response. The registered response handlers are invoked with
 *  response once the PLDM responder sends the response. If no response is
 *  received within the instance ID expiration interval or any other failure the
 *  response handler is invoked with the empty response.
 *
 * @tparam RequestInterface - Request class type
 */
template <class RequestInterface>
class Handler
{

  public:
    Handler() = delete;
    Handler(const Handler&) = delete;
    Handler(Handler&&) = delete;
    Handler& operator=(const Handler&) = delete;
    Handler& operator=(Handler&&) = delete;
    ~Handler() = default;

    /** @brief Constructor
     *
     *  @param[in] fd - fd of MCTP communications socket
     *  @param[in] event - reference to PLDM daemon's main event loop
     *  @param[in] requester - reference to Requester object
     *  @param[in] currentSendbuffSize - current send buffer size
     *  @param[in] verbose - verbose tracing flag
     *  @param[in] instanceIdExpiryInterval - instance ID expiration interval
     *  @param[in] numRetries - number of request retries
     *  @param[in] responseTimeOut - time to wait between each retry
     */
    explicit Handler(
        int fd, sdeventplus::Event& event, pldm::dbus_api::Requester& requester,
        int currentSendbuffSize, bool verbose,
        std::chrono::seconds instanceIdExpiryInterval =
            std::chrono::seconds(INSTANCE_ID_EXPIRATION_INTERVAL),
        uint8_t numRetries = static_cast<uint8_t>(NUMBER_OF_REQUEST_RETRIES),
        std::chrono::milliseconds responseTimeOut =
            std::chrono::milliseconds(RESPONSE_TIME_OUT)) :
        fd(fd),
        event(event), requester(requester),
        currentSendbuffSize(currentSendbuffSize), verbose(verbose),
        instanceIdExpiryInterval(instanceIdExpiryInterval),
        numRetries(numRetries), responseTimeOut(responseTimeOut)
    {}

    /** @brief Register a PLDM request message
     *
     *  @param[in] eid - endpoint ID of the remote MCTP endpoint
     *  @param[in] instanceId - instance ID to match request and response
     *  @param[in] type - PLDM type
     *  @param[in] command - PLDM command
     *  @param[in] requestMsg - PLDM request message
     *  @param[in] responseHandler - Response handler for this request
     *
     *  @return return PLDM_SUCCESS on success and PLDM_ERROR otherwise
     */
    int registerRequest(mctp_eid_t eid, uint8_t instanceId, uint8_t type,
                        uint8_t command, pldm::Request&& requestMsg,
                        ResponseHandler&& responseHandler)
    {
        RequestKey key{eid, instanceId, type, command};

        auto instanceIdExpiryCallBack = [key, this](void) {
            if (this->handlers.contains(key))
            {
                std::cerr << "Response not received for the request, instance "
                             "ID expired."
                          << " EID = " << (unsigned)key.eid
                          << " INSTANCE_ID = " << (unsigned)key.instanceId
                          << " TYPE = " << (unsigned)key.type
                          << " COMMAND = " << (unsigned)key.command << "\n";
                auto& [request, responseHandler, timerInstance] =
                    this->handlers[key];
                request->stop();
                auto rc = timerInstance->stop();
                if (rc)
                {
                    std::cerr
                        << "Failed to stop the instance ID expiry timer. RC = "
                        << rc << "\n";
                }
                // Call response handler with an empty response to indicate no
                // response
                responseHandler(key.eid, nullptr, 0);
                this->removeRequestContainer.emplace(
                    key, std::make_unique<sdeventplus::source::Defer>(
                             event, std::bind(&Handler::removeRequestEntry,
                                              this, key)));
            }
            else
            {
                // This condition is not possible, if a response is received
                // before the instance ID expiry, then the response handler
                // is executed and the entry will be removed.
                assert(false);
            }
        };

        auto request = std::make_unique<RequestInterface>(
            fd, eid, event, std::move(requestMsg), numRetries, responseTimeOut,
            currentSendbuffSize, verbose);
        auto timer = std::make_unique<phosphor::Timer>(
            event.get(), instanceIdExpiryCallBack);

        auto rc = request->start();
        if (rc)
        {
            requester.markFree(eid, instanceId);
            std::cerr << "Failure to send the PLDM request message"
                      << "\n";
            return rc;
        }

        try
        {
            timer->start(duration_cast<std::chrono::microseconds>(
                instanceIdExpiryInterval));
        }
        catch (const std::runtime_error& e)
        {
            requester.markFree(eid, instanceId);
            std::cerr << "Failed to start the instance ID expiry timer. RC = "
                      << e.what() << "\n";
            return PLDM_ERROR;
        }

        handlers.emplace(key, std::make_tuple(std::move(request),
                                              std::move(responseHandler),
                                              std::move(timer)));
        return rc;
    }

    /** @brief Handle PLDM response message
     *
     *  @param[in] eid - endpoint ID of the remote MCTP endpoint
     *  @param[in] instanceId - instance ID to match request and response
     *  @param[in] type - PLDM type
     *  @param[in] command - PLDM command
     *  @param[in] response - PLDM response message
     *  @param[in] respMsgLen - length of the response message
     */
    void handleResponse(mctp_eid_t eid, uint8_t instanceId, uint8_t type,
                        uint8_t command, const pldm_msg* response,
                        size_t respMsgLen)
    {
        RequestKey key{eid, instanceId, type, command};
        if (handlers.contains(key))
        {
            auto& [request, responseHandler, timerInstance] = handlers[key];
            request->stop();
            auto rc = timerInstance->stop();
            if (rc)
            {
                std::cerr
                    << "Failed to stop the instance ID expiry timer. RC = "
                    << rc << "\n";
            }
            responseHandler(eid, response, respMsgLen);
            requester.markFree(key.eid, key.instanceId);
            handlers.erase(key);
        }
        else
        {
            // Got a response for a PLDM request message not registered with the
            // request handler, so freeing up the instance ID, this can be other
            // OpenBMC applications relying on PLDM D-Bus apis like
            // openpower-occ-control and softoff
            requester.markFree(key.eid, key.instanceId);
        }
    }

  private:
    int fd; //!< file descriptor of MCTP communications socket
    sdeventplus::Event& event; //!< reference to PLDM daemon's main event loop
    pldm::dbus_api::Requester& requester; //!< reference to Requester object
    int currentSendbuffSize;              //!< current Send Buffer size
    bool verbose;                         //!< verbose tracing flag
    std::chrono::seconds
        instanceIdExpiryInterval; //!< Instance ID expiration interval
    uint8_t numRetries;           //!< number of request retries
    std::chrono::milliseconds
        responseTimeOut; //!< time to wait between each retry

    /** @brief Container for storing the details of the PLDM request
     *         message, handler for the corresponding PLDM response and the
     *         timer object for the Instance ID expiration
     */
    using RequestValue =
        std::tuple<std::unique_ptr<RequestInterface>, ResponseHandler,
                   std::unique_ptr<phosphor::Timer>>;

    /** @brief Container for storing the PLDM request entries */
    std::unordered_map<RequestKey, RequestValue, RequestKeyHasher> handlers;

    /** @brief Container to store information about the request entries to be
     *         removed after the instance ID timer expires
     */
    std::unordered_map<RequestKey, std::unique_ptr<sdeventplus::source::Defer>,
                       RequestKeyHasher>
        removeRequestContainer;

    /** @brief Remove request entry for which the instance ID expired
     *
     *  @param[in] key - key for the Request
     */
    void removeRequestEntry(RequestKey key)
    {
        if (removeRequestContainer.contains(key))
        {
            removeRequestContainer[key].reset();
            requester.markFree(key.eid, key.instanceId);
            handlers.erase(key);
            removeRequestContainer.erase(key);
        }
    }
};

struct sendRecvPldmMsg
{
    std::coroutine_handle<> resumeHandle;
    requester::Handler<requester::Request>& handler;
    uint8_t eid;
    pldm::Request& requestMsg;
    pldm::Response& responseMsg;
    uint8_t rc;

    bool await_ready() noexcept
    {
        return false;
    }

    bool await_suspend(std::coroutine_handle<> handle) noexcept
    {
        resumeHandle = handle;

        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        rc = handler.registerRequest(
            eid, request->hdr.instance_id, request->hdr.type,
            request->hdr.command, std::move(requestMsg),
            std::move(std::bind_front(&sendRecvPldmMsg::HandleResponse, this)));
        if (rc)
        {
            std::cerr << "registerRequest failed, rc="
                      << static_cast<unsigned>(rc) << "\n";
            return false;
        }
        return true;
    }

    uint8_t await_resume() const noexcept
    {
        return rc;
    }

    sendRecvPldmMsg(requester::Handler<requester::Request>& handler,
                    uint8_t eid, pldm::Request& requestMsg,
                    pldm::Response& responseMsg) :
        handler(handler),
        eid(eid), requestMsg(requestMsg), responseMsg(responseMsg)
    {
        rc = PLDM_SUCCESS;
        responseMsg.clear();
    }

    void HandleResponse(mctp_eid_t eid, const pldm_msg* response, size_t length)
    {
        if (response == nullptr || !length)
        {
            std::cerr << "No response received, EID=" << unsigned(eid) << "\n";
            rc = PLDM_ERROR;
        }
        else
        {
            const uint8_t* responsePtr =
                reinterpret_cast<const uint8_t*>(response);

            responseMsg.insert(responseMsg.end(), responsePtr,
                               responsePtr + length +
                                   sizeof(struct pldm_msg_hdr));
        }
        resumeHandle();
    }
};

struct Coroutine
{
    struct promise_type
    {
        std::coroutine_handle<> parent_handle;
        uint8_t data;

        Coroutine get_return_object()
        {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend()
        {
            return {};
        }

        auto final_suspend() noexcept
        {
            struct awaiter
            {
                bool await_ready() const noexcept
                {
                    return false;
                }

                void await_resume() const noexcept
                {}

                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept
                {
                    auto parent_handle = h.promise().parent_handle;
                    if (parent_handle)
                    {
                        return parent_handle;
                    }
                    return std::noop_coroutine();
                }
            };

            return awaiter{};
        }

        void unhandled_exception()
        {}

        void return_value(uint8_t value) noexcept
        {
            data = std::move(value);
        }
    };

    bool await_ready() const noexcept
    {
        return handle.done();
    }

    uint8_t await_resume() const noexcept
    {
        return std::move(handle.promise().data);
    }

    bool await_suspend(std::coroutine_handle<> coroutine)
    {
        handle.promise().parent_handle = coroutine;
        return true;
    }

    mutable std::coroutine_handle<promise_type> handle;
};

} // namespace requester

} // namespace pldm
