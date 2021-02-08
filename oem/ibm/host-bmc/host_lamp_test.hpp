#pragma once

#include "pldmd/dbus_impl_requester.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Led/Group/server.hpp>

#include <string>

using namespace pldm::dbus_api;

namespace pldm
{
namespace led
{

/** @class HostLampTest
 *  @brief Manages group of LEDs and applies action on the elements of group
 */
class HostLampTest :
    sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::Led::server::Group>
{
  public:
    HostLampTest() = delete;
    ~HostLampTest() = default;
    HostLampTest(const HostLampTest&) = delete;
    HostLampTest& operator=(const HostLampTest&) = delete;
    HostLampTest(HostLampTest&&) = default;
    HostLampTest& operator=(HostLampTest&&) = default;

    /** @brief Constructs LED Group
     *
     * @param[in] bus       - Handle to system dbus
     * @param[in] objPath   - The D-Bus path that hosts LED group
     * @param[in] mctp_fd   - MCTP file descriptor
     * @param[in] mctp_eid  - MCTP terminal ID
     */
    HostLampTest(sdbusplus::bus::bus& bus, const std::string& objPath,
                 int mctp_fd, uint8_t mctp_eid, Requester& requester) :
        sdbusplus::server::object::object<
            sdbusplus::xyz::openbmc_project::Led::server::Group>(
            bus, objPath.c_str(), true),
        bus(bus), path(objPath), mctp_fd(mctp_fd), mctp_eid(mctp_eid),
        requester(requester)
    {
        // Emit deferred signal.
        emit_object_added();
    }

    /** @brief Property SET Override function
     *
     *  @param[in]  value   -  True or False
     *  @return             -  Success or exception thrown
     */
    bool asserted(bool value) override;

  private:
    /** @brief sdbusplus handler */
    sdbusplus::bus::bus& bus;

    /** @brief Path of the group instance */
    std::string path;

    /** @brief fd of MCTP communications socket */
    int mctp_fd;

    /** @brief MCTP EID of host firmware */
    uint8_t mctp_eid;

    /** @brief reference to Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    Requester& requester;

    /** @brief Get effecterID from PDRs.
     *
     *  @return effecterID
     */
    uint16_t getEffecterID();

    /** @brief Send set state effecter states to PHYP.
     *
     *  @param[in]  effecterID   -  effecterID
     *
     *  @return PLDM completion codes
     */
    int sendHostStateEffecter(uint16_t effecterID);
};

} // namespace led
} // namespace pldm
