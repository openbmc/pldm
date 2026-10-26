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
    std::expected<PeerId, std::string_view> select(
        const Endpoint&, const std::map<PeerId, Endpoint>& inUse) const override
    {
        for (unsigned peer = 1; peer < 255; peer++)
        {
            if (!inUse.contains(static_cast<PeerId>(peer)))
            {
                return static_cast<PeerId>(peer);
            }
        }
        return std::unexpected("No free PeerId");
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
        if (failUnmap)
        {
            return -1;
        }
        mapped.erase(peer);
        return 0;
    }

    bool failMap = false;
    bool failUnmap = false;
    std::map<PeerId, Endpoint> mapped;
};

} // namespace pldm
