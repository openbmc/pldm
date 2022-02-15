#pragma once

#include "libpldm/base.h"
#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

#include "boost/core/ignore_unused.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/device_handler.hpp"
#include "requester/handler.hpp"
#include "requester/request.hpp"

#include <sdeventplus/event.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <vector>

namespace pldm
{

namespace device
{

/** @class Manager
 *
 * This class manages the PLDM devices
 */
class Manager
{

  public:
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager& operator=(Manager&&) = delete;
    ~Manager() = default;

    /** @brief Constructor
     *
     *  @param[in] handler - PLDM request handler
     */
    explicit Manager(
        int socket, sdbusplus::bus::bus& bus, sdeventplus::Event& event,
        pldm_pdr* repo, pldm_entity_association_tree* entityTree,
        pldm_entity_association_tree* bmcEntityTree,
        pldm::dbus_api::Requester& requester,
        pldm::requester::Handler<pldm::requester::Request>* handler) :
        mctp_fd(socket),
        bus(bus), event(event), repo(repo), entityTree(entityTree),
        bmcEntityTree(bmcEntityTree), requester(requester), handler(handler)
    {}

    /** @brief Add the discovered MCTP endpoints to the managed devices list
     *
     *  @param[in] eids - Array of MCTP endpoints
     *
     *  @return return PLDM_SUCCESS
     */
    int addDevices(const std::vector<mctp_eid_t>& eids)
    {
        for (const auto& it : eids)
        {
            std::cerr << "Adding Device EID : " << unsigned(it) << std::endl;
            auto dev = new DevHandler(mctp_fd, it, event, bus, repo, entityTree,
                                      bmcEntityTree, requester, handler);
            mDevices.insert(std::pair<mctp_eid_t, DevHandler*>(it, dev));
        }
        return PLDM_SUCCESS;
    }

    /** @brief Remove the MCTP devices from the managed devices list
     *
     *  @param[in] eids - Array of MCTP endpoints
     *
     *  @return return PLDM_SUCCESS
     */
    int removeDevices(const std::vector<mctp_eid_t>& eids)
    {
        for (const auto& it : eids)
        {
            if (!mDevices.count(it))
            {
                continue;
            }
            mDevices.erase(it);
        }
        return PLDM_SUCCESS;
    }

  private:
    /** @brief fd of MCTP communications socket */
    int mctp_fd;

    /** @brief reference of main D-bus interface of pldmd devices */
    sdbusplus::bus::bus& bus;

    /** @brief reference of main event loop of pldmd, primarily used to schedule
     *  work.
     */
    sdeventplus::Event& event;

    /** @brief pointer to BMC's primary PDR repo, devives PDRs are added here */
    pldm_pdr* repo;

    /** @brief Pointer to BMC's and Host's entity association tree */
    pldm_entity_association_tree* entityTree;

    /** @brief Pointer to BMC's entity association tree */
    pldm_entity_association_tree* bmcEntityTree;

    /** @brief reference to Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    pldm::dbus_api::Requester& requester;

    /** @brief PLDM request handler */
    pldm::requester::Handler<pldm::requester::Request>* handler;

    /** @brief List MCTP devices which are managerd by device manager */
    std::map<mctp_eid_t, DevHandler*> mDevices;
};

}; // namespace device

} // namespace pldm