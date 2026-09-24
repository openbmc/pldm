#include "common/transport.hpp"

#include <libpldm/instance-id.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>

namespace
{
unsigned outstandingInstances = 0;
unsigned exchanges = 0;
} // namespace

extern "C" int __wrap_pldm_instance_db_init_default(pldm_instance_db** ctx)
{
    const int descriptor = memfd_create("pldmtool-test", MFD_CLOEXEC);
    if (descriptor < 0)
    {
        return -1;
    }
    if (ftruncate(descriptor, 256 * 32) != 0)
    {
        close(descriptor);
        return -1;
    }
    const auto path = "/proc/self/fd/" + std::to_string(descriptor);
    const int rc = pldm_instance_db_init(ctx, path.c_str());
    close(descriptor);
    return rc;
}

extern "C" int __real_pldm_instance_id_alloc(pldm_instance_db*, pldm_tid_t,
                                             pldm_instance_id_t*);
extern "C" int __real_pldm_instance_id_free(pldm_instance_db*, pldm_tid_t,
                                            pldm_instance_id_t);
extern "C" int __real_pldm_instance_db_destroy(pldm_instance_db*);

extern "C" int __wrap_pldm_instance_id_alloc(
    pldm_instance_db* ctx, pldm_tid_t tid, pldm_instance_id_t* instance)
{
    const auto requested = std::getenv("PLDM_TEST_INSTANCE_ID");
    const auto initial = requested ? std::stoul(requested) : 10;
    for (unsigned count = 0; count < initial; ++count)
    {
        int rc = __real_pldm_instance_id_alloc(ctx, tid, instance);
        if (rc != 0)
        {
            return rc;
        }
        rc = __real_pldm_instance_id_free(ctx, tid, *instance);
        if (rc != 0)
        {
            return rc;
        }
    }
    const int rc = __real_pldm_instance_id_alloc(ctx, tid, instance);
    if (rc == 0)
    {
        ++outstandingInstances;
        std::cerr << "TEST_ALLOC=" << static_cast<unsigned>(*instance) << '\n';
    }
    return rc;
}

extern "C" int __wrap_pldm_instance_id_free(
    pldm_instance_db* ctx, pldm_tid_t tid, pldm_instance_id_t instance)
{
    const int rc = __real_pldm_instance_id_free(ctx, tid, instance);
    if (rc == 0)
    {
        --outstandingInstances;
        std::cerr << "TEST_FREE=" << static_cast<unsigned>(instance) << '\n';
    }
    return rc;
}

extern "C" int __wrap_pldm_instance_db_destroy(pldm_instance_db* ctx)
{
    if (outstandingInstances != 0)
    {
        std::cerr
            << "Instance ID was not released before command destruction\n";
        std::abort();
    }
    return __real_pldm_instance_db_destroy(ctx);
}

PldmTransport::PldmTransport(bool)
{
    if (!std::getenv("PLDM_TEST_RESPONSE"))
    {
        throw std::runtime_error("Test transport requires PLDM_TEST_RESPONSE");
    }
}

PldmTransport::~PldmTransport() = default;

pldm_requester_rc_t PldmTransport::sendRecvMsg(
    pldm_tid_t tid, const void* tx, size_t txLen, void*& rx, size_t& rxLen)
{
    const auto bytes = static_cast<const uint8_t*>(tx);
    std::cerr << "TEST_REQUEST=";
    for (size_t index = 0; index < txLen; ++index)
    {
        std::cerr << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<unsigned>(bytes[index]);
    }
    std::cerr << " EID=" << std::dec << static_cast<unsigned>(tid) << '\n';

    const std::string mode = std::getenv("PLDM_TEST_RESPONSE");
    if (mode == "transport-error" || (mode == "retry-once" && exchanges++ == 0))
    {
        return PLDM_REQUESTER_RECV_FAIL;
    }
    rxLen = mode == "empty"          ? 3
            : mode == "extra"        ? 5
            : mode == "short-header" ? 2
                                     : 4;
    rx = std::calloc(rxLen, 1);
    if (!rx)
    {
        throw std::bad_alloc();
    }
    auto response = static_cast<uint8_t*>(rx);
    std::memcpy(response, bytes, std::min(rxLen, sizeof(pldm_msg_hdr)));
    response[0] &= 0x1f;
    if (rxLen > sizeof(pldm_msg_hdr))
    {
        response[3] = mode == "device-error" ? 0x80 : 0;
    }
    return PLDM_REQUESTER_SUCCESS;
}
