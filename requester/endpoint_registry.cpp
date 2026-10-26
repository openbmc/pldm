#include "endpoint_registry.hpp"

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace
{
// EIDs 0 to 7 are reserved and 255 is the broadcast EID (DSP0236 v1.3.3 Table
// 2)
constexpr eid maxValidEid = 254;

bool isUsableEid(eid endpointId)
{
    return endpointId >= MCTP_START_VALID_EID && endpointId <= maxValidEid;
}
} // namespace

std::optional<PeerId> PooledPeerIdPolicy::select(
    const Endpoint& endpoint, const std::map<PeerId, Endpoint>& inUse) const
{
    if (!isUsableEid(endpoint.endpointId))
    {
        return std::nullopt;
    }

    if (!inUse.contains(endpoint.endpointId))
    {
        return endpoint.endpointId;
    }

    for (unsigned peer = maxValidEid; peer >= MCTP_START_VALID_EID; peer--)
    {
        if (!inUse.contains(static_cast<PeerId>(peer)))
        {
            return static_cast<PeerId>(peer);
        }
    }
    return std::nullopt;
}

std::optional<PeerId> IdentityPeerIdPolicy::select(
    const Endpoint& endpoint, const std::map<PeerId, Endpoint>& inUse) const
{
    if (!isUsableEid(endpoint.endpointId))
    {
        return std::nullopt;
    }

    PeerId peer = endpoint.endpointId;
    if (inUse.contains(peer))
    {
        return std::nullopt;
    }
    return peer;
}

EndpointRegistry::EndpointRegistry(std::unique_ptr<PeerIdPolicy> policy,
                                   AddressMapper* mapper) :
    policy(std::move(policy)), mapper(mapper)
{}

std::optional<PeerId> EndpointRegistry::acquire(const Endpoint& endpoint)
{
    if (auto existing = peerOf(endpoint))
    {
        return existing;
    }

    auto peer = policy->select(endpoint, byPeer);
    if (!peer)
    {
        return std::nullopt;
    }

    if (mapper)
    {
        int rc = mapper->map(*peer, endpoint);
        if (rc != 0)
        {
            error(
                "Cannot bind PeerId {PEER} to {ENDPOINT} in the transport, error {RC}",
                "PEER", static_cast<unsigned>(*peer), "ENDPOINT",
                formatEndpoint(endpoint), "RC", rc);
            return std::nullopt;
        }
    }

    byPeer.emplace(*peer, endpoint);
    byEndpoint.emplace(endpoint, *peer);
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
                "PEER", static_cast<unsigned>(peer), "ENDPOINT",
                formatEndpoint(endpoint), "RC", rc);
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
    return "PeerId " + std::to_string(static_cast<unsigned>(peer));
}

} // namespace pldm
