#pragma once

#include "libpldm/base.h"
#include "libpldm/platform.h"

#include "common/types.hpp"
#include "common/utils.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace pldm
{

namespace terminus
{

/** @class TerminusHandler
 *  @brief This class handle the functional of a PLDM device
 *  @details Provides an APIs to manage one PLDM device. The supported APIs can
 *  be "GetDevicePDRs", "CreateSensors", "ReadingSensors"
 */
class TerminusHandler
{
  public:
    /** @brief Constructor
     *  @param[in] tid - Temimus EID of host firmware
     *  @param[in] event - reference of main event loop of pldmd
     *  @param[in] repo - pointer to BMC's primary PDR repo
     *  @param[in] eventsJsonDir - directory path which has the config JSONs
     *  @param[in] entityTree - Pointer to BMC and Host entity association tree
     *  @param[in] bmcEntityTree - pointer to BMC's entity association tree
     *  @param[in] requester - reference to Requester object
     *  @param[in] handler - PLDM request handler
     */
    explicit TerminusHandler(
        uint8_t tid, sdeventplus::Event& event, sdbusplus::bus::bus& bus,
        pldm_pdr* repo, pldm_entity_association_tree* entityTree,
        pldm_entity_association_tree* bmcEntityTree,
        pldm::dbus_api::Requester& requester,
        pldm::requester::Handler<pldm::requester::Request>* handler);

    ~TerminusHandler();

  private:
    /** @brief Terminus EID of host firmware */
    uint8_t tid;
    /** @brief reference of main D-bus interface of pldmd devices */
    sdbusplus::bus::bus& bus;

    /** @brief reference of main event loop of pldmd, primarily used to schedule
     *  work.
     */
    sdeventplus::Event& event;
    /** @brief pointer to BMC's primary PDR repo, host PDRs are added here */
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
};

} // namespace terminus

} // namespace pldm
