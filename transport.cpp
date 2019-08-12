#include "pldmd_api.hpp"

#include <unistd.h>

namespace pldm
{
namespace daemon_api
{

int Transport::send(const void* buf, size_t count)
{
    return write(sockFd, buf, count);
}

int Transport::sendPLDMMsg(uint8_t destinationId, std::vector<uint8_t>& msg)
{
    msg.insert(msg.begin(), MCTP_MSG_TYPE_PLDM);
    msg.insert(msg.begin(), destinationId);
    return send(msg.data(), msg.size());
}

} // namespace daemon_api
} // namespace pldm
