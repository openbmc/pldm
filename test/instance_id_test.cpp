#include "test/test_instance_id.hpp"

#include <chrono>

#include <gtest/gtest.h>

TEST(TestInstanceIds, NoTimeout)
{
    TestInstanceIdDb db;
    for (int i = 0; i < 32; i++)
    {
        auto iid = db.next(12);
        ASSERT_TRUE(iid.has_value());
        EXPECT_LT(iid.value(), 32);
    }
    {
        auto iid = db.next(12);
        EXPECT_FALSE(iid.has_value());
        EXPECT_EQ(iid.error().rc(), -EAGAIN);
        EXPECT_EQ(
            iid.error().msg(),
            "Failed to allocate instance ID for EID 12: No free instance ids");
    }
    db.free(12, 0);
    {
        auto iid = db.next(12);
        ASSERT_TRUE(iid.has_value());
        EXPECT_EQ(iid.value(), 0);
    }
    EXPECT_NO_THROW(db.free(12, 24));
    EXPECT_THROW(db.free(12, 24), std::runtime_error);
}

TEST(TestInstanceIds, Timeout)
{
    using namespace std::literals;
    TestInstanceIdDb db;
    for (int i = 0; i < 32; i++)
    {
        auto iid = db.next(12);
        ASSERT_TRUE(iid.has_value());
        EXPECT_LT(iid.value(), 32);
    }
    {
        auto iid = db.next(12);
        EXPECT_FALSE(iid.has_value());
    }
    auto tid = std::thread([&]() {
        std::this_thread::sleep_for(100ms);
        db.free(12, 0);
    });
    {
        auto iid = db.next(12, 1);
        ASSERT_TRUE(iid.has_value());
        EXPECT_EQ(iid.value(), 0);
    }
    tid.join();
}
