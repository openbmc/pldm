#pragma once

#include "common/transport.hpp"
#include "requester/endpoint_registry.hpp"

namespace pldm
{

/** @brief Binds a PeerId to its MCTP Network and EID in the transport */
class TransportAddressMapper : public AddressMapper
{
  public:
    explicit TransportAddressMapper(PldmTransport& transport) :
        transport(transport)
    {}

    int map(PeerId peer, const Endpoint& endpoint) override
    {
        return transport.mapEndpoint(peer, endpoint);
    }

    int unmap(PeerId peer) override
    {
        return transport.unmapEndpoint(peer);
    }

  private:
    PldmTransport& transport;
};

} // namespace pldm
