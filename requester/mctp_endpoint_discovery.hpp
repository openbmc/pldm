#pragma once

#include <sdbusplus/bus/match.hpp>

#include "libpldm/requester/pldm.h"
#include <initializer_list>
#include <vector>


namespace pldm
{

class IMctpDiscoveryHandler
{
  public:
    virtual void handleMCTPEndpoints(const std::vector<mctp_eid_t>& eids) = 0;
    virtual ~IMctpDiscoveryHandler()
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
     *  @param[in] list - initializer list to the IMctpDiscoveryHandlers
     */
    explicit MctpDiscovery(sdbusplus::bus::bus& bus,
                           std::initializer_list<IMctpDiscoveryHandler*> list);

    void loadStaticEndpoints(const std::string& jsonPath);

  private:
    /** @brief reference to the systemd bus */
    sdbusplus::bus::bus& bus;

    std::vector<IMctpDiscoveryHandler*> handlers;

    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointSignal;

    void dicoverEndpoints(sdbusplus::message::message& msg);

    static constexpr uint8_t mctpTypePLDM = 1;

    static constexpr std::string_view mctpEndpointIntfName{
        "xyz.openbmc_project.MCTP.Endpoint"};
};

} // namespace pldm