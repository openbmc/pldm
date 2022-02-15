#pragma once

#include "fw-update/manager.hpp"

#include <sdbusplus/bus/match.hpp>

namespace pldm
{

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
     *  @param[in] fwManager - pointer to the firmware manager
     */
    explicit MctpDiscovery(sdbusplus::bus::bus& bus,
                           fw_update::Manager* fwManager);

  private:
    /** @brief reference to the systemd bus */
    sdbusplus::bus::bus& bus;

    fw_update::Manager* fwManager;

    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointAddedSignal;

    /** @brief Used to watch for the removed MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointRemovedSignal;

    void discoverEndpoints(sdbusplus::message::message& msg);

    void removeEndpoints(sdbusplus::message::message& msg);

    static constexpr uint8_t mctpTypePLDM = 1;

    static constexpr std::string_view mctpEndpointIntfName{
        "xyz.openbmc_project.MCTP.Endpoint"};

    /* List MCTP endpoint in MCTP D-Bus interface or Static EID table */
    std::vector<mctp_eid_t> listEids;
};

} // namespace pldm