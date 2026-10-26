#pragma once

#include "common/types.hpp"

#include <compare>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace pldm
{

/** @brief A remote MCTP endpoint as the MCTP daemon identifies it: an EID is
 *         only unique within an MCTP Network.
 */
struct Endpoint
{
    NetworkId networkId;
    eid endpointId;

    auto operator<=>(const Endpoint&) const = default;
};

/** @brief An MCTP endpoint together with the PeerId it is addressed by */
struct MctpInfoWithPeer
{
    PeerId peer;
    MctpInfo mctpInfo;
};

using MctpInfoWithPeers = std::vector<MctpInfoWithPeer>;

/** @brief Drop the PeerIds from a list of endpoints
 *
 *  @param[in] endpoints - MCTP endpoints with their PeerIds
 *
 *  @return the MctpInfo of each endpoint, in the same order
 */
inline MctpInfos toMctpInfos(const MctpInfoWithPeers& endpoints)
{
    MctpInfos mctpInfos;
    for (const auto& endpoint : endpoints)
    {
        mctpInfos.emplace_back(endpoint.mctpInfo);
    }
    return mctpInfos;
}

/** @brief Extract the Endpoint from an MctpInfo
 *
 *  @param[in] mctpInfo - MCTP endpoint information
 *
 *  @return the Network and EID of the endpoint
 */
inline Endpoint toEndpoint(const MctpInfo& mctpInfo)
{
    return Endpoint{std::get<NetworkId>(mctpInfo), std::get<eid>(mctpInfo)};
}

/** @brief Human readable form of an Endpoint for log messages
 *
 *  @param[in] endpoint - the endpoint to format
 *
 *  @return string such as "Network 1 EID 10", the Network first
 */
inline std::string formatEndpoint(const Endpoint& endpoint)
{
    return std::format("Network {} EID {}", endpoint.networkId,
                       endpoint.endpointId);
}

} // namespace pldm
