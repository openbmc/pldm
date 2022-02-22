#include "platform-mc/terminus.hpp"

#include <gtest/gtest.h>

TEST(TerminusTest, supportedTypeTest)
{
    auto t1 = pldm::platform_mc::Terminus(1, 1 << PLDM_BASE);
    auto t2 = pldm::platform_mc::Terminus(2,
                                          1 << PLDM_BASE | 1 << PLDM_PLATFORM);

    EXPECT_EQ(true, t1.doesSupportType(PLDM_BASE));
    EXPECT_EQ(false, t1.doesSupportType(PLDM_PLATFORM));
    EXPECT_EQ(true, t2.doesSupportType(PLDM_BASE));
    EXPECT_EQ(true, t2.doesSupportType(PLDM_PLATFORM));
}

TEST(TerminusTest, getTidTest)
{
    const pldm_tid_t tid = 1;
    auto t1 = pldm::platform_mc::Terminus(tid, 1 << PLDM_BASE);

    EXPECT_EQ(tid, t1.getTid());
}
