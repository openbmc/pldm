#include "endpoint_registry.hpp"

#include <phosphor-logging/lg2.hpp>

#include <expected>
#include <format>
#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace
{
// EIDs 0 to 7 are reserved and 255 is the broadcast EID ( Table
// 2)

bool isUsableEid(eid endpointId)
{
    return endpointId >= MCTP_START_VALID_EID &&
           endpointId <= MCTP_END_VALID_EID;
}
} // namespace

std::expected<PeerId, std::string_view> PooledPeerIdPolicy::select(
    const Endpoint& endpoint, const std::map<PeerId, Endpoint>& inUse) const
{
    if (!isUsableEid(endpoint.endpointId))
    {
        return std::unexpected("EID outside the usable range");
    }

    if (!inUse.contains(endpoint.endpointId))
    {
        return endpoint.endpointId;
    }

    for (unsigned peer = MAX_PEER_ID; peer >= MIN_PEER_ID; peer--)
    {
        if (!inUse.contains(static_cast<PeerId>(peer)))
        {
            return static_cast<PeerId>(peer);
        }
    }
    return std::unexpected("No free PeerId in the pool");
}

std::expected<PeerId, std::string_view> IdentityPeerIdPolicy::select(
    const Endpoint& endpoint, const std::map<PeerId, Endpoint>& inUse) const
{
    if (!isUsableEid(endpoint.endpointId))
    {
        return std::unexpected("EID outside the usable range");
    }

    PeerId peer = endpoint.endpointId;
    if (inUse.contains(peer))
    {
        return std::unexpected("EID already in use by another endpoint");
    }
    return peer;
}

EndpointRegistry::EndpointRegistry(std::unique_ptr<PeerIdPolicy> policy,
                                   AddressMapper* mapper) :
    policy(std::move(policy)), mapper(mapper)
{}

std::expected<PeerId, std::string> EndpointRegistry::acquire(
    const Endpoint& endpoint)
{
    if (auto existing = peerOf(endpoint))
    {
        return *existing;
    }

    auto selectedPeer = policy->select(endpoint, byPeer);
    if (!selectedPeer)
    {
        return std::unexpected(
            std::format("Cannot select PeerId for {}: {}",
                        formatEndpoint(endpoint), selectedPeer.error()));
    }
    auto peer = *selectedPeer;

    if (mapper)
    {
        int rc = mapper->map(peer, endpoint);
        if (rc != 0)
        {
            return std::unexpected(std::format(
                "Cannot bind PeerId {} to {} in the transport, error {}", peer,
                formatEndpoint(endpoint), rc));
        }
    }

    byPeer.emplace(peer, endpoint);
    byEndpoint.emplace(endpoint, peer);
    return peer;
}

void EndpointRegistry::release(const Endpoint& endpoint)
{
    auto it = byEndpoint.find(endpoint);
    if (it == byEndpoint.end())
    {
        return;
    }

    auto peer = it->second;
    if (mapper)
    {
        // The PeerId is given back either way. A mapping that is left behind
        // is replaced the next time the PeerId is bound.
        int rc = mapper->unmap(peer);
        if (rc != 0)
        {
            error(
                "Cannot unbind PeerId {PEER} of {ENDPOINT} in the transport, error {RC}",
                "PEER", peer, "ENDPOINT", formatEndpoint(endpoint), "RC", rc);
        }
    }
    byPeer.erase(peer);
    byEndpoint.erase(it);
}

std::optional<PeerId> EndpointRegistry::peerOf(const Endpoint& endpoint) const
{
    auto it = byEndpoint.find(endpoint);
    if (it == byEndpoint.end())
    {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Endpoint> EndpointRegistry::endpointOf(PeerId peer) const
{
    auto it = byPeer.find(peer);
    if (it == byPeer.end())
    {
        return std::nullopt;
    }
    return it->second;
}

std::string EndpointRegistry::describe(PeerId peer) const
{
    if (auto endpoint = endpointOf(peer))
    {
        return formatEndpoint(*endpoint);
    }
    return std::format("PeerId {}", peer);
}

} // namespace pldm
