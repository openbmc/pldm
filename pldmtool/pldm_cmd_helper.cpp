#include "config.h"

#include "pldm_cmd_helper.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/pldm.h>
#include <libpldm/transport.h>
#include <libpldm/transport/af-mctp.h>
#include <libpldm/transport/mctp-demux.h>
#include <poll.h>
#include <systemd/sd-bus.h>

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <exception>

using namespace pldm::utils;

namespace pldmtool
{
namespace helper
{

union TransportImpl
{
    struct pldm_transport_mctp_demux* mctp_demux;
    struct pldm_transport_af_mctp* af_mctp;
};

[[maybe_unused]] static struct pldm_transport*
    pldm_transport_impl_mctp_demux_init(TransportImpl& impl, pldm_tid_t& tid,
                                        mctp_eid_t eid, pollfd& pollfd)
{
    impl.mctp_demux = NULL;
    pldm_transport_mctp_demux_init(&impl.mctp_demux);
    if (!impl.mctp_demux)
    {
        return nullptr;
    }

    pldm_transport_mctp_demux_map_tid(impl.mctp_demux, tid, eid);

    pldm_transport* pldmTransport =
        pldm_transport_mctp_demux_core(impl.mctp_demux);

    pldm_transport_mctp_demux_init_pollfd(pldmTransport, &pollfd);

    return pldmTransport;
}

[[maybe_unused]] static struct pldm_transport*
    pldm_transport_impl_af_mctp_init(TransportImpl& impl, pldm_tid_t& tid,
                                     mctp_eid_t eid, pollfd& pollfd)
{
    impl.af_mctp = NULL;
    pldm_transport_af_mctp_init(&impl.af_mctp);
    if (!impl.af_mctp)
    {
        return nullptr;
    }

    pldm_transport_af_mctp_map_tid(impl.af_mctp, tid, eid);

    pldm_transport* pldmTransport = pldm_transport_af_mctp_core(impl.af_mctp);

    pldm_transport_af_mctp_init_pollfd(pldmTransport, &pollfd);

    return pldmTransport;
}

static struct pldm_transport* transport_impl_init(TransportImpl& impl,
                                                  pldm_tid_t& tid,
                                                  mctp_eid_t eid,
                                                  pollfd& pollfd)
{
#if defined(PLDM_TRANSPORT_WITH_MCTP_DEMUX)
    return pldm_transport_impl_mctp_demux_init(impl, tid, eid, pollfd);
#elif defined(PLDM_TRANSPORT_WITH_AF_MCTP)
    return pldm_transport_impl_af_mctp_init(impl, tid, eid, pollfd);
#else
    return nullptr;
#endif
}

static void transport_impl_destroy(TransportImpl& impl)
{
#if defined(PLDM_TRANSPORT_WITH_MCTP_DEMUX)
    pldm_transport_mctp_demux_destroy(impl.mctp_demux);
#elif defined(PLDM_TRANSPORT_WITH_AF_MCTP)
    pldm_transport_af_mctp_destroy(impl.af_mctp);
#endif
}

void CommandInterface::exec()
{
    static constexpr auto pldmObjPath = "/xyz/openbmc_project/pldm";
    static constexpr auto pldmRequester = "xyz.openbmc_project.PLDM.Requester";
    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        auto service = pldm::utils::DBusHandler().getService(pldmObjPath,
                                                             pldmRequester);
        auto method = bus.new_method_call(service.c_str(), pldmObjPath,
                                          pldmRequester, "GetInstanceId");
        method.append(mctp_eid);
        auto reply = bus.call(
            method,
            std::chrono::duration_cast<microsec>(sec(DBUS_TIMEOUT)).count());
        reply.read(instanceId);
    }
    catch (const std::exception& e)
    {
        std::cerr << "GetInstanceId D-Bus call failed, MCTP id = "
                  << (unsigned)mctp_eid << ", error = " << e.what() << "\n";
        return;
    }
    auto [rc, requestMsg] = createRequestMsg();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to encode request message for " << pldmType << ":"
                  << commandName << " rc = " << rc << "\n";
        return;
    }

    std::vector<uint8_t> responseMsg;
    rc = pldmSendRecv(requestMsg, responseMsg);

    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "pldmSendRecv: Failed to receive RC = " << rc << "\n";
        return;
    }

    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsg.data());
    parseResponseMsg(responsePtr, responseMsg.size() - sizeof(pldm_msg_hdr));
}

int CommandInterface::pldmSendRecv(std::vector<uint8_t>& requestMsg,
                                   std::vector<uint8_t>& responseMsg)
{
    // By default enable request/response msgs for pldmtool raw commands.
    if (CommandInterface::pldmType == "raw")
    {
        pldmVerbose = true;
    }

    if (pldmVerbose)
    {
        std::cout << "pldmtool: ";
        printBuffer(Tx, requestMsg);
    }

    void* responseMessage = nullptr;
    size_t responseMessageSize{};
    auto tid = mctp_eid;
    pollfd pollfd;
    TransportImpl transportImpl;
    pldm_transport* pldmTransport = transport_impl_init(transportImpl, tid,
                                                        mctp_eid, pollfd);
    if (pldmTransport == nullptr)
    {
        std::cerr << "Failed to init transport impl\n";
        exit(EXIT_FAILURE);
    }

    int rc = pldm_transport_send_recv_msg(pldmTransport, tid, requestMsg.data(),
                                          requestMsg.size(), &responseMessage,
                                          &responseMessageSize);
    if (rc)
    {
        std::cerr << "failed to pldm send recv\n";
        return rc;
    }

    responseMsg.resize(responseMessageSize);
    memcpy(responseMsg.data(), responseMessage, responseMsg.size());

    free(responseMessage);
    transport_impl_destroy(transportImpl);

    if (pldmVerbose)
    {
        std::cout << "pldmtool: ";
        printBuffer(Rx, responseMsg);
    }
    return PLDM_SUCCESS;
}
} // namespace helper
} // namespace pldmtool
