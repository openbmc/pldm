#pragma once

#include "common/endpoint.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace pldm
{

/** @brief Strategy that picks the PeerId for an endpoint */
class PeerIdPolicy
{
  public:
    virtual ~PeerIdPolicy() = default;

    /** @brief Choose a PeerId for an endpoint that has none yet
     *
     *  @param[in] endpoint - the endpoint to choose a PeerId for
     *  @param[in] inUse - the PeerIds already handed out and their endpoints
     *
     *  @return the chosen PeerId, std::nullopt if none can be chosen
     */
    virtual std::optional<PeerId> select(
        const Endpoint& endpoint,
        const std::map<PeerId, Endpoint>& inUse) const = 0;
};

/** @brief PeerId equals the EID, as the transport maps the EIDs itself.
 *         Endpoints with the same EID on different Networks cannot be told
 *         apart, so the second one is rejected.
 */
class IdentityPeerIdPolicy : public PeerIdPolicy
{
  public:
    std::optional<PeerId> select(
        const Endpoint& endpoint,
        const std::map<PeerId, Endpoint>& inUse) const override;
};

/** @brief PeerId equals the EID when that value is free, as with the
 *         identity policy, so a single Network looks exactly like it does
 *         with the identity policy. An endpoint whose EID is taken by one on
 *         another Network gets the highest free PeerId instead. EIDs are
 *         handed out from the low end up, so this takes the values a later
 *         endpoint is least likely to need for itself.
 */
class PooledPeerIdPolicy : public PeerIdPolicy
{
  public:
    std::optional<PeerId> select(
        const Endpoint& endpoint,
        const std::map<PeerId, Endpoint>& inUse) const override;
};

/** @brief Binds a PeerId to the endpoint it addresses in the transport */
class AddressMapper
{
  public:
    virtual ~AddressMapper() = default;

    /** @return 0 on success, otherwise an error code */
    virtual int map(PeerId peer, const Endpoint& endpoint) = 0;

    /** @return 0 on success, otherwise an error code */
    virtual int unmap(PeerId peer) = 0;
};

/** @class EndpointRegistry
 *
 *  Tracks which PeerId stands for which MCTP endpoint. It is the one place
 *  where a (Network, EID) pair is turned into the key used by the transport,
 *  the requester handler and the instance ID database, and back.
 */
class EndpointRegistry
{
  public:
    EndpointRegistry(const EndpointRegistry&) = delete;
    EndpointRegistry& operator=(const EndpointRegistry&) = delete;

    /** @brief Construct the registry
     *
     *  @param[in] policy - how PeerIds are chosen
     *  @param[in] mapper - binds a new PeerId in the transport. May be null
     *                      when the transport needs no binding, e.g. the
     *                      identity mappings it creates itself.
     */
    explicit EndpointRegistry(std::unique_ptr<PeerIdPolicy> policy,
                              AddressMapper* mapper = nullptr);

    /** @brief Get the PeerId of an endpoint, assigning one if it has none
     *
     *  @param[in] endpoint - the endpoint
     *
     *  @return the PeerId, std::nullopt if none could be assigned or the
     *          mapper failed. A mapper failure is logged with its error code.
     */
    std::optional<PeerId> acquire(const Endpoint& endpoint);

    /** @brief Forget an endpoint and unmap its PeerId. Unknown endpoints are
     *         ignored. The endpoint is forgotten, and its PeerId given back,
     *         also when the mapper cannot unmap it. That is logged.
     *
     *  @param[in] endpoint - the endpoint
     */
    void release(const Endpoint& endpoint);

    /** @brief Get the PeerId of a known endpoint */
    std::optional<PeerId> peerOf(const Endpoint& endpoint) const;

    /** @brief Get the endpoint behind a PeerId */
    std::optional<Endpoint> endpointOf(PeerId peer) const;

    /** @brief Name an endpoint by its PeerId for log messages
     *
     *  @return e.g. "Network 1 EID 10", or "PeerId 10" if it is not known
     */
    std::string describe(PeerId peer) const;

  private:
    std::unique_ptr<PeerIdPolicy> policy;
    AddressMapper* mapper;
    std::map<PeerId, Endpoint> byPeer;
    std::map<Endpoint, PeerId> byEndpoint;
};

} // namespace pldm
