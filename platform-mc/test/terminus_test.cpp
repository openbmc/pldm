#include "platform-mc/terminus.hpp"

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

TEST(TerminusTest, supportedTypeTest)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE);
    auto t2 = Terminus(2, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);

    EXPECT_EQ(true, t1.doesSupport(PLDM_BASE));
    EXPECT_EQ(false, t1.doesSupport(PLDM_PLATFORM));
    EXPECT_EQ(true, t2.doesSupport(PLDM_BASE));
    EXPECT_EQ(true, t2.doesSupport(PLDM_PLATFORM));
}

TEST(TerminusTest, getTidTest)
{
    const pldm_tid_t tid = 1;
    auto t1 = Terminus(tid, 1 << PLDM_BASE);

    EXPECT_EQ(tid, t1.getTid());
}
