#pragma once

#include "libpldm/requester/pldm.h"

#include <sdbusplus/bus/match.hpp>

#include <filesystem>
#include <initializer_list>
#include <vector>

namespace pldm
{

using EID = uint8_t;
using UUID = std::string;
using MctpInfo = std::pair<EID, UUID>;
using MctpInfos = std::vector<MctpInfo>;

/** @class MctpDiscoveryHandlerIntf
 *
 * This abstract class defines the APIs for MctpDiscovery class has common
 * interface to execute function from different Manager Classes
 */
class MctpDiscoveryHandlerIntf
{
  public:
    virtual void handleMCTPEndpoints(const MctpInfos& mctpInfos) = 0;
    virtual ~MctpDiscoveryHandlerIntf()
    {}
};

class MctpDiscovery
{
  public:
    MctpDiscovery() = delete;
    MctpDiscovery(const MctpDiscovery&) = delete;
    MctpDiscovery(MctpDiscovery&&) = delete;
    MctpDiscovery& operator=(const MctpDiscovery&) = delete;
    MctpDiscovery& operator=(MctpDiscovery&&) = delete;
    ~MctpDiscovery() = default;

    /** @brief Constructs the MCTP Discovery object to handle discovery of
     *         MCTP enabled devices
     *
     *  @param[in] bus - reference to systemd bus
     *  @param[in] list - initializer list to the MctpDiscoveryHandlerIntf
     */
    explicit MctpDiscovery(
        sdbusplus::bus::bus& bus,
        std::initializer_list<MctpDiscoveryHandlerIntf*> list);

    void loadStaticEndpoints(const std::filesystem::path& jsonPath);

  private:
    /** @brief reference to the systemd bus */
    sdbusplus::bus::bus& bus;

    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointSignal;

    std::vector<MctpDiscoveryHandlerIntf*> handlers;

    void dicoverEndpoints(sdbusplus::message::message& msg);

    void handleMCTPEndpoints(const MctpInfos& mctpInfos);

    static constexpr uint8_t mctpTypePLDM = 1;

    static constexpr std::string_view mctpEndpointIntfName{
        "xyz.openbmc_project.MCTP.Endpoint"};
};

} // namespace pldm