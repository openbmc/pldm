#pragma once

#include <libpldm/transport/af-mctp.h>
#include <libpldm/transport/mctp-demux.h>

struct pollfd;

union TransportImpl
{
    struct pldm_transport_mctp_demux* mctp_demux;
    struct pldm_transport_af_mctp* af_mctp;
};

struct pldm_transport* transport_impl_init(TransportImpl& impl, pollfd& pollfd);
void transport_impl_destroy(TransportImpl& impl);
