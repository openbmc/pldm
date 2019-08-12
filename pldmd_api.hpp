#pragma once

#include <memory>
#include <vector>

constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;

namespace pldm
{
namespace daemon_api
{

class Transport;

struct Interfaces
{
    std::unique_ptr<Transport> transport{};
};

class Transport
{
  public:
    Transport(int sockFd) : sockFd(sockFd)
    {
    }

    int sendPLDMMsg(uint8_t destinationId, std::vector<uint8_t>& msg);
    int send(const void* buf, size_t count);

  private:
    int sockFd;
};

} // namespace daemon_api
} // namespace pldm
