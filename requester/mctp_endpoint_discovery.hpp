#pragma once

#include "libpldm/pldm.h"

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
    virtual void handleMctpEndpoints(const MctpInfos& mctpInfos) = 0;
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
        std::initializer_list<MctpDiscoveryHandlerIntf*> list,
        const std::filesystem::path& staticEidTablePath =
            STATIC_EID_TABLE_PATH);

  private:
    /** @brief reference to the systemd bus */
    sdbusplus::bus_t& bus;

    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointAddedSignal;

    /** @brief Used to watch for the removed MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointRemovedSignal;

    /** @brief List of handlers need to notify when new MCTP
     * Endpoint is Added/Removed */
    std::vector<MctpDiscoveryHandlerIntf*> handlers;

    /** @brief Path of static EID table config file */
    std::filesystem::path staticEidTablePath;

    /** @brief Callback function when new MCTP endpoints or removed endpoints
     * D-Bus signal raised.
     *
     *  @param[in] msg - Data associated with subscribed signal
     */
    void discoverEndpoints(sdbusplus::message_t& msg);

    /** @brief Helper function to invoke registered handlers
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos);

    /** @brief Loading the static MCTP endpoints to mctpInfos.
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void loadStaticEndpoints(MctpInfos& mctpInfos);

    static constexpr uint8_t mctpTypePLDM = 1;

    static constexpr std::string_view mctpEndpointIntfName{
        "xyz.openbmc_project.MCTP.Endpoint"};
};

} // namespace pldm
