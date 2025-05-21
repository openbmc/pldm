#include "test/test_instance_id.hpp"

#include <chrono>

#include <gtest/gtest.h>

TEST(TestInstanceIds, NoTimeout)
{
    TestInstanceIdDb db;
    for (int i = 0; i < 32; i++)
    {
        auto iid = db.next(12);
        ASSERT_LT(iid, 32);
    }
    EXPECT_THROW(db.next(12), std::runtime_error);
    db.free(12, 0);
    EXPECT_EQ(db.next(12), 0);
    db.free(12, 24);
    EXPECT_THROW(db.free(12, 24), std::runtime_error);
}

TEST(TestInstanceIds, Timeout)
{
    using namespace std::literals;
    TestInstanceIdDb db;
    for (int i = 0; i < 32; i++)
    {
        auto iid = db.next(12);
        ASSERT_LT(iid, 32);
    }
    EXPECT_THROW(db.next(12), std::runtime_error);
    auto tid = std::thread([&]() {
        std::this_thread::sleep_for(100ms);
        db.free(12, 0);
    });
    ASSERT_EQ(db.next(12, 1), 0);
    tid.join();
}
