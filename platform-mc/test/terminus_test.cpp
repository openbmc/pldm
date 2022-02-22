#include "platform-mc/terminus.hpp"

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

TEST(TerminusTest, supportedTypeTest)
{
    auto t1 = Terminus(0, 1, 1 << PLDM_BASE);
    auto t2 = Terminus(1, 2, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);

    EXPECT_EQ(true, t1.doesSupport(PLDM_BASE));
    EXPECT_EQ(false, t1.doesSupport(PLDM_PLATFORM));
    EXPECT_EQ(true, t2.doesSupport(PLDM_BASE));
    EXPECT_EQ(true, t2.doesSupport(PLDM_PLATFORM));
}
