#include "common/transport.hpp"

#include <algorithm>
#include <ranges>

/*
 * Currently the OpenBMC ecosystem assumes TID == EID. Pre-populate the TID
 * mappings over the EID space excluding the Null (0), Reserved (1 to 7),
 * Broadcast EIDs (255) defined by Section 8.2 Special endpoint IDs in DSP0236
 * v1.3.1. Further, by Section 8.1.1 SetTID command (0x01) in DSP0240 v1.1.0,
 * the TIDs 0x00 and 0xff are also reserved. These overlap with the reserved
 * EIDs so no additional filtering is required.
 *
 * Further, pldmtool and pldmd are two separate processes. They are opening two
 * different sockets, but with the mctp-demux-daemon, the response messages are
 * broadcasted to all sockets. When pldmd receives the response for a request
 * issued by pldmtool, pldm_transport_mctp_demux_recv() may return with error
 * PLDM_REQUESTER_RECV_FAIL if it fails to map the EID of the source endpoint to
 * its TID. The EID to TID mappings of pldmtool and pldmd should be coherent to
 * prevent the failure of pldm_transport_mctp_demux_recv().
 */

[[maybe_unused]] static struct pldm_transport*
    pldm_transport_impl_mctp_demux_init(TransportImpl& impl, pollfd& pollfd)
{
    impl.mctp_demux = NULL;
    pldm_transport_mctp_demux_init(&impl.mctp_demux);
    if (!impl.mctp_demux)
    {
        return nullptr;
    }

    for (auto i : std::views::iota(8, 255))
    {
        int rc = pldm_transport_mctp_demux_map_tid(impl.mctp_demux, i, i);
        if (rc)
        {
            pldm_transport_af_mctp_destroy(impl.af_mctp);
            return nullptr;
        }
    }

    pldm_transport* pldmTransport =
        pldm_transport_mctp_demux_core(impl.mctp_demux);

    pldm_transport_mctp_demux_init_pollfd(pldmTransport, &pollfd);

    return pldmTransport;
}

[[maybe_unused]] static struct pldm_transport*
    pldm_transport_impl_af_mctp_init(TransportImpl& impl, pollfd& pollfd)
{
    impl.af_mctp = NULL;
    pldm_transport_af_mctp_init(&impl.af_mctp);
    if (!impl.af_mctp)
    {
        return nullptr;
    }

    for (auto i : std::views::iota(8, 255))
    {
        int rc = pldm_transport_af_mctp_map_tid(impl.af_mctp, i, i);
        if (rc)
        {
            pldm_transport_af_mctp_destroy(impl.af_mctp);
            return nullptr;
        }
    }

    /* Listen for requests on any interface */
    if (pldm_transport_af_mctp_bind(impl.af_mctp, nullptr, 0))
    {
        return nullptr;
    }

    pldm_transport* pldmTransport = pldm_transport_af_mctp_core(impl.af_mctp);

    pldm_transport_af_mctp_init_pollfd(pldmTransport, &pollfd);

    return pldmTransport;
}

struct pldm_transport* transport_impl_init(TransportImpl& impl, pollfd& pollfd)
{
#if defined(PLDM_TRANSPORT_WITH_MCTP_DEMUX)
    return pldm_transport_impl_mctp_demux_init(impl, pollfd);
#elif defined(PLDM_TRANSPORT_WITH_AF_MCTP)
    return pldm_transport_impl_af_mctp_init(impl, pollfd);
#else
    return nullptr;
#endif
}

void transport_impl_destroy(TransportImpl& impl)
{
#if defined(PLDM_TRANSPORT_WITH_MCTP_DEMUX)
    pldm_transport_mctp_demux_destroy(impl.mctp_demux);
#elif defined(PLDM_TRANSPORT_WITH_AF_MCTP)
    pldm_transport_af_mctp_destroy(impl.af_mctp);
#endif
}
