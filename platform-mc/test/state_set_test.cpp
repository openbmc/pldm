#include "common/utils.hpp"
#include "platform-mc/state_set.hpp"
#include "platform-mc/state_set_link_state.hpp"

#include <libpldm/state_set.h>

#include <memory>
#include <string>

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

TEST(StateSetTest, createStateSetTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/state_set";
    auto portIntf = std::make_shared<PortIntf>(bus, path.c_str());

    /* The link state set has a D-Bus interface */
    EXPECT_NE(nullptr,
              createStateSet(path, portIntf, PLDM_STATE_SET_LINK_STATE));

    /* A state set whose interface is not added yet has none */
    EXPECT_EQ(nullptr, createStateSet(path, portIntf,
                                      PLDM_STATE_SET_CONFIGURATION_STATE));
}

TEST(StateSetTest, createLinkStateWithoutPortTest)
{
    std::string path = "/xyz/openbmc_project/inventory/test/state_set_no_port";

    /* An entity whose type does not implement Inventory.Connector.Port has
     * nowhere to publish the link status
     */
    EXPECT_EQ(nullptr,
              createStateSet(path, nullptr, PLDM_STATE_SET_LINK_STATE));
}

TEST(StateSetTest, linkStateStatusTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/link_status";
    auto portIntf = std::make_shared<PortIntf>(bus, path.c_str());
    StateSetLinkState stateSet(path, portIntf);

    /* The interface carries the link status unknown until a component sensor
     * reports a state
     */
    EXPECT_EQ(LinkStatusValue::Unknown, stateSet.linkStatus());

    /* Each reading replaces the previous one, so the link comes back when the
     * terminus reports it connected again
     */
    stateSet.setPresentState(PLDM_STATE_SET_LINK_STATE_DISCONNECTED);
    EXPECT_EQ(LinkStatusValue::Down, stateSet.linkStatus());

    stateSet.setPresentState(PLDM_STATE_SET_LINK_STATE_CONNECTED);
    EXPECT_EQ(LinkStatusValue::Up, stateSet.linkStatus());

    stateSet.setPresentState(PLDM_STATE_SET_LINK_STATE_DISCONNECTED);
    EXPECT_EQ(LinkStatusValue::Down, stateSet.linkStatus());
}

TEST(StateSetTest, linkStateSharesThePortInterfaceTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/link_shared_port";
    auto portIntf = std::make_shared<PortIntf>(bus, path.c_str());
    StateSetLinkState stateSet(path, portIntf);

    /* The state set publishes on the interface the entity D-Bus object
     * already implements, so the entity does not implement it twice
     */
    stateSet.setPresentState(PLDM_STATE_SET_LINK_STATE_CONNECTED);
    EXPECT_EQ(LinkStatusValue::Up, portIntf->linkStatus());
}

TEST(StateSetTest, linkStateUnknownStateTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path =
        "/xyz/openbmc_project/inventory/test/link_status_unknown";
    auto portIntf = std::make_shared<PortIntf>(bus, path.c_str());
    StateSetLinkState stateSet(path, portIntf);

    /* A state the state set does not define keeps the link status of the last
     * reading a state value was defined for
     */
    stateSet.setPresentState(PLDM_STATE_SET_LINK_STATE_DISCONNECTED);
    stateSet.setPresentState(0xff);
    EXPECT_EQ(LinkStatusValue::Down, stateSet.linkStatus());

    stateSet.setPresentState(PLDM_STATE_SET_LINK_STATE_CONNECTED);
    stateSet.setPresentState(0xff);
    EXPECT_EQ(LinkStatusValue::Up, stateSet.linkStatus());
}

TEST(StateSetTest, stateSetsShareOneInterfaceTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/link_shared";
    auto portIntf = std::make_shared<PortIntf>(bus, path.c_str());
    StateSets stateSets(path, portIntf);

    /* The component sensors which report the same state set of the same
     * entity share one interface
     */
    auto* first = stateSets.getStateSet(PLDM_STATE_SET_LINK_STATE);
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(first, stateSets.getStateSet(PLDM_STATE_SET_LINK_STATE));

    /* A state set with no D-Bus interface publishes nothing */
    EXPECT_EQ(nullptr,
              stateSets.getStateSet(PLDM_STATE_SET_CONFIGURATION_STATE));
}
