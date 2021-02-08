#pragma once

#include "pldmd/dbus_impl_pdr.hpp"
#include "pldmd/dbus_impl_requester.hpp"

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Led/Group/server.hpp>

#include <string>

using namespace pldm::dbus_api;

namespace pldm
{
namespace led
{

using LEDGroupObj = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Led::server::Group>;

class HostLampTestInterfaces
{
  public:
    virtual ~HostLampTestInterfaces()
    {}

    virtual uint16_t getEffecterID() = 0;
    virtual int setHostStateEffecter(uint16_t effecterID) = 0;
};

/** @class HostLampTest
 *  @brief Manages group of PHYP lamp test LEDs
 */
class HostLampTest : public HostLampTestInterfaces, public LEDGroupObj
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
     * @param[in] requester - Reference to Requester object
     * @param[in] repo      - pointer to BMC's primary PDR repo
     */
    HostLampTest(sdbusplus::bus::bus& bus, const std::string& objPath,
                 int mctp_fd, uint8_t mctp_eid, Requester& requester,
                 const pldm_pdr* repo) :
        LEDGroupObj(bus, objPath.c_str()),
        path(objPath), mctp_fd(mctp_fd), mctp_eid(mctp_eid),
        requester(requester), pdrRepo(repo)
    {}

    /** @brief Property SET Override function
     *
     *  @param[in]  value   -  True or False
     *  @return             -  Success or exception thrown
     */
    bool asserted(bool value) override;

    /** @brief Property GET Override function
     *
     *  @return             -  true / false
     */
    bool asserted() const override;

    /** @brief Get effecterID from PDRs.
     *
     *  @return effecterID
     */
    uint16_t getEffecterID() override;

    /** @brief Set state effecter states to PHYP.
     *
     *  @param[in]  effecterID   -  effecterID
     *
     *  @return PLDM completion codes
     */
    int setHostStateEffecter(uint16_t effecterID) override;

  private:
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

    /** @brief pointer to BMC's primary PDR repo */
    const pldm_pdr* pdrRepo;
};

} // namespace led
} // namespace pldm
