#pragma once

#include "common/flight_recorder.hpp"
#include "common/transport.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>
#include <sys/socket.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>

#include <chrono>
#include <functional>
#include <iostream>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace requester
{
/** @class RequestRetryTimer
 *
 *  The abstract base class for implementing the PLDM request retry logic. This
 *  class handles number of times the PLDM request needs to be retried if the
 *  response is not received and the time to wait between each retry. It
 *  provides APIs to start and stop the request flow.
 */
class RequestRetryTimer
{
  public:
    RequestRetryTimer() = delete;
    RequestRetryTimer(const RequestRetryTimer&) = delete;
    RequestRetryTimer(RequestRetryTimer&&) = delete;
    RequestRetryTimer& operator=(const RequestRetryTimer&) = delete;
    RequestRetryTimer& operator=(RequestRetryTimer&&) = delete;
    virtual ~RequestRetryTimer() = default;

    /** @brief Constructor
     *
     *  @param[in] event - reference to PLDM daemon's main event loop
     *  @param[in] numRetries - number of request retries
     *  @param[in] timeout - time to wait between each retry in milliseconds
     */
    explicit RequestRetryTimer(sdeventplus::Event& event, uint8_t numRetries,
                               std::chrono::milliseconds timeout) :

        event(event), numRetries(numRetries), timeout(timeout),
        timer(event.get(), [this] { this->callback(); })
    {}

    /** @brief Starts the request flow and arms the timer for request retries
     *
     *  @return return PLDM_SUCCESS on success and PLDM_ERROR otherwise
     */
    int start()
    {
        auto rc = send();
        if (rc)
        {
            return rc;
        }

        try
        {
            if (numRetries)
            {
                timer.start(duration_cast<std::chrono::microseconds>(timeout),
                            true);
            }
        }
        catch (const std::runtime_error& e)
        {
            error("Failed to start the request timer, error - {ERROR}", "ERROR",
                  e);
            return PLDM_ERROR;
        }

        return PLDM_SUCCESS;
    }

    /** @brief Stops the timer and no further request retries happen */
    void stop()
    {
        auto rc = timer.stop();
        if (rc)
        {
            error("Failed to stop the request timer, response code '{RC}'",
                  "RC", rc);
        }
    }

  protected:
    sdeventplus::Event& event; //!< reference to PLDM daemon's main event loop
    uint8_t numRetries;        //!< number of request retries
    std::chrono::milliseconds
        timeout;            //!< time to wait between each retry in milliseconds
    sdbusplus::Timer timer; //!< manages starting timers and handling timeouts

    /** @brief Sends the PLDM request message
     *
     *  @return return PLDM_SUCCESS on success and PLDM_ERROR otherwise
     */
    virtual int send() const = 0;

    /** @brief Callback function invoked when the timeout happens */
    void callback()
    {
        if (numRetries--)
        {
            send();
        }
        else
        {
            stop();
        }
    }
};

/** @class Request
 *
 *  The concrete implementation of RequestIntf. This class implements the send()
 *  to send the PLDM request message over MCTP socket.
 *  This class encapsulates the PLDM request message, the number of times the
 *  request needs to retried if the response is not received and the amount of
 *  time to wait between each retry. It provides APIs to start and stop the
 *  request flow.
 */
class Request final : public RequestRetryTimer
{
  public:
    Request() = delete;
    Request(const Request&) = delete;
    Request(Request&&) = delete;
    Request& operator=(const Request&) = delete;
    Request& operator=(Request&&) = delete;
    ~Request() = default;

    /** @brief Constructor
     *
     *  @param[in] pldm_transport - PLDM transport object
     *  @param[in] eid - endpoint ID of the remote MCTP endpoint
     *  @param[in] currrentSendbuffSize - the current send buffer size
     *  @param[in] event - reference to PLDM daemon's main event loop
     *  @param[in] requestMsg - PLDM request message
     *  @param[in] numRetries - number of request retries
     *  @param[in] timeout - time to wait between each retry in milliseconds
     *  @param[in] verbose - verbose tracing flag
     */
    explicit Request(PldmTransport* pldmTransport, mctp_eid_t eid,
                     sdeventplus::Event& event, pldm::Request&& requestMsg,
                     uint8_t numRetries, std::chrono::milliseconds timeout,
                     bool verbose) :
        RequestRetryTimer(event, numRetries, timeout),
        pldmTransport(pldmTransport), eid(eid),
        requestMsg(std::move(requestMsg)), verbose(verbose)
    {}

  private:
    PldmTransport* pldmTransport; //!< PLDM transport
    mctp_eid_t eid;               //!< endpoint ID of the remote MCTP endpoint
    pldm::Request requestMsg;     //!< PLDM request message
    bool verbose;                 //!< verbose tracing flag

    /** @brief Sends the PLDM request message on the socket
     *
     *  @return return PLDM_SUCCESS on success and PLDM_ERROR otherwise
     */
    int send() const
    {
        if (verbose)
        {
            pldm::utils::printBuffer(pldm::utils::Tx, requestMsg);
        }
        pldm::flightrecorder::FlightRecorder::GetInstance().saveRecord(
            requestMsg, true);
        const struct pldm_msg_hdr* hdr =
            (struct pldm_msg_hdr*)(requestMsg.data());
        if (!hdr->request)
        {
            return PLDM_REQUESTER_NOT_REQ_MSG;
        }

        if (pldmTransport == nullptr)
        {
            error("Invalid transport: Unable to send PLDM request");
            return PLDM_ERROR;
        }

        auto rc = pldmTransport->sendMsg(static_cast<pldm_tid_t>(eid),
                                         requestMsg.data(), requestMsg.size());
        if (rc < 0)
        {
            error(
                "Failed to send pldmTransport message, response code '{RC}' and error - {ERROR}",
                "RC", rc, "ERROR", errno);
            return PLDM_ERROR;
        }
        return PLDM_SUCCESS;
    }
};

} // namespace requester

} // namespace pldm
