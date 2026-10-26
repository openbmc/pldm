#pragma once

#include "requester/endpoint_registry.hpp"

namespace pldm
{

/** @brief Test policy that hands out the lowest free PeerId, so the same EID
 *         can be used on more than one Network.
 */
class LowestFreePeerIdPolicy : public PeerIdPolicy
{
  public:
    std::optional<PeerId> select(
        const Endpoint&, const std::map<PeerId, Endpoint>& inUse) const override
    {
        for (unsigned peer = 1; peer < 255; peer++)
        {
            if (!inUse.contains(static_cast<PeerId>(peer)))
            {
                return static_cast<PeerId>(peer);
            }
        }
        return std::nullopt;
    }
};

/** @brief Test mapper that records what is bound, instead of a transport */
class FakeMapper : public AddressMapper
{
  public:
    int map(PeerId peer, const Endpoint& endpoint) override
    {
        if (failMap)
        {
            return -1;
        }
        mapped[peer] = endpoint;
        return 0;
    }

    int unmap(PeerId peer) override
    {
        mapped.erase(peer);
        return 0;
    }

    bool failMap = false;
    std::map<PeerId, Endpoint> mapped;
};

} // namespace pldm
