#include "platform-mc/state_set.hpp"

#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <optional>

#include <gtest/gtest.h>

using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;

TEST(StateSets, shareInterface)
{
    pldm::platform_mc::StateSets stateSets(
        "/xyz/openbmc_project/inventory/system/Entity_1");

    auto& first = stateSets.getInterface<OperationalStatusIntf>();
    auto& second = stateSets.getInterface<OperationalStatusIntf>();
    EXPECT_EQ(&first, &second);
}

TEST(StateSets, initInterfaceOnce)
{
    pldm::platform_mc::StateSets stateSets(
        "/xyz/openbmc_project/inventory/system/Entity_2");

    auto& intf = stateSets.getInterface<OperationalStatusIntf>(
        [](OperationalStatusIntf& i) { i.functional(true); });
    EXPECT_TRUE(intf.functional());

    stateSets.getInterface<OperationalStatusIntf>([](OperationalStatusIntf& i) {
        i.functional(false);
    });
    EXPECT_TRUE(intf.functional());
}

TEST(StateSets, effecterControlsStateSetOnce)
{
    pldm::platform_mc::StateSets stateSets(
        "/xyz/openbmc_project/inventory/system/Entity_3");

    constexpr pldm::pdr::StateSetId controlled = 1;
    constexpr pldm::pdr::StateSetId uncontrolled = 2;

    EXPECT_TRUE(stateSets.addEffecterStateSet(controlled));
    EXPECT_FALSE(stateSets.addEffecterStateSet(controlled));
    EXPECT_TRUE(stateSets.isEffecterStateSet(controlled));
    EXPECT_FALSE(stateSets.isEffecterStateSet(uncontrolled));
}

namespace
{

/** @brief A state set whose writable property requests the state it is set
 *         to
 */
class WritableStateSet : public pldm::platform_mc::StateSetBase
{
  public:
    void setPresentState(uint8_t) override {}

    void setProperty(uint8_t state)
    {
        requestState(state);
    }
};

} // namespace

TEST(StateSetBase, requestWithoutEffecterIsNotAllowed)
{
    WritableStateSet stateSet;
    EXPECT_THROW(stateSet.setProperty(1),
                 sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed);
}

TEST(StateSetBase, requestIsHandedToEffecter)
{
    WritableStateSet stateSet;
    std::optional<uint8_t> requested;
    stateSet.setStateRequester([&requested](uint8_t state) {
        requested = state;
    });

    stateSet.setProperty(2);
    EXPECT_EQ(2, requested);
}
