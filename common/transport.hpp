#pragma once

#include <libpldm/base.h>
#include <libpldm/requester/pldm.h>
#include <poll.h>
#include <stddef.h>

struct pldm_transport_mctp_demux;
struct pldm_transport_af_mctp;

union TransportImpl
{
    struct pldm_transport_mctp_demux* mctp_demux;
    struct pldm_transport_af_mctp* af_mctp;
};

/* RAII for pldm_transport */
class PldmTransport
{
  public:
    PldmTransport();
    PldmTransport(const PldmTransport& other) = delete;
    PldmTransport(const PldmTransport&& other) = delete;
    PldmTransport& operator=(const PldmTransport& other) = delete;
    PldmTransport& operator=(const PldmTransport&& other) = delete;
    ~PldmTransport();

    int getEventSource() const;
    pldm_requester_rc_t sendMsg(pldm_tid_t tid, const void* tx, size_t len);
    pldm_requester_rc_t recvMsg(pldm_tid_t& tid, void*& rx, size_t& len);
    pldm_requester_rc_t sendRecvMsg(pldm_tid_t tid, const void* tx,
                                    size_t txLen, void*& rx, size_t& rxLen);

  private:
    pollfd pfd;
    TransportImpl impl;
    struct pldm_transport* transport;
};
