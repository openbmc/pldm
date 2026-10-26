#include "requester/endpoint_registry.hpp"
#include "requester/test/test_peer_id_policy.hpp"

#include <gtest/gtest.h>

using namespace pldm;

namespace
{

EndpointRegistry makeIdentityRegistry()
{
    return EndpointRegistry(std::make_unique<IdentityPeerIdPolicy>());
}

} // namespace

TEST(EndpointRegistryTest, IdentityUsesEidAsPeerId)
{
    auto registry = makeIdentityRegistry();

    EXPECT_EQ(registry.acquire(Endpoint{1, 10}), 10);
    EXPECT_EQ(registry.peerOf(Endpoint{1, 10}), 10);
    EXPECT_EQ(registry.endpointOf(10), (Endpoint{1, 10}));
}

TEST(EndpointRegistryTest, AcquireIsIdempotent)
{
    auto registry = makeIdentityRegistry();

    EXPECT_EQ(registry.acquire(Endpoint{1, 10}), 10);
    EXPECT_EQ(registry.acquire(Endpoint{1, 10}), 10);
}

TEST(EndpointRegistryTest, IdentityRejectsSameEidOnAnotherNetwork)
{
    auto registry = makeIdentityRegistry();

    EXPECT_EQ(registry.acquire(Endpoint{1, 10}), 10);
    EXPECT_EQ(registry.acquire(Endpoint{2, 10}), std::nullopt);
    EXPECT_EQ(registry.peerOf(Endpoint{2, 10}), std::nullopt);
    EXPECT_EQ(registry.endpointOf(10), (Endpoint{1, 10}));
}

TEST(EndpointRegistryTest, IdentityRejectsReservedEids)
{
    auto registry = makeIdentityRegistry();

    EXPECT_EQ(registry.acquire(Endpoint{1, 0}), std::nullopt);
    EXPECT_EQ(registry.acquire(Endpoint{1, 7}), std::nullopt);
    EXPECT_EQ(registry.acquire(Endpoint{1, 255}), std::nullopt);
    EXPECT_EQ(registry.acquire(Endpoint{1, 8}), 8);
    EXPECT_EQ(registry.acquire(Endpoint{1, 254}), 254);
}

TEST(EndpointRegistryTest, ReleaseFreesThePeerId)
{
    auto registry = makeIdentityRegistry();

    registry.acquire(Endpoint{1, 10});
    registry.release(Endpoint{1, 10});

    EXPECT_EQ(registry.peerOf(Endpoint{1, 10}), std::nullopt);
    EXPECT_EQ(registry.endpointOf(10), std::nullopt);
    EXPECT_EQ(registry.acquire(Endpoint{2, 10}), 10);
}

TEST(EndpointRegistryTest, ReleaseOfUnknownEndpointIsIgnored)
{
    auto registry = makeIdentityRegistry();

    registry.release(Endpoint{1, 10});

    EXPECT_EQ(registry.peerOf(Endpoint{1, 10}), std::nullopt);
}

TEST(EndpointRegistryTest, DescribeNamesTheEndpoint)
{
    auto registry = makeIdentityRegistry();
    registry.acquire(Endpoint{3, 20});

    EXPECT_EQ(registry.describe(20), "Network 3 EID 20");
    EXPECT_EQ(registry.describe(21), "PeerId 21");
}

TEST(EndpointRegistryTest, PolicyDecidesThePeerId)
{
    EndpointRegistry registry(std::make_unique<LowestFreePeerIdPolicy>());

    auto a = registry.acquire(Endpoint{1, 10});
    auto b = registry.acquire(Endpoint{2, 10});

    ASSERT_TRUE(a && b);
    EXPECT_NE(*a, *b);
    EXPECT_EQ(registry.endpointOf(*a), (Endpoint{1, 10}));
    EXPECT_EQ(registry.endpointOf(*b), (Endpoint{2, 10}));
}

TEST(EndpointRegistryTest, MapperIsCalledOnAcquireAndRelease)
{
    FakeMapper mapper;
    EndpointRegistry registry(std::make_unique<LowestFreePeerIdPolicy>(),
                              &mapper);

    auto peer = registry.acquire(Endpoint{2, 10});
    ASSERT_TRUE(peer);
    EXPECT_EQ(mapper.mapped.at(*peer), (Endpoint{2, 10}));

    registry.release(Endpoint{2, 10});
    EXPECT_TRUE(mapper.mapped.empty());
}

TEST(EndpointRegistryTest, MapperIsNotCalledAgainForAKnownEndpoint)
{
    FakeMapper mapper;
    EndpointRegistry registry(std::make_unique<LowestFreePeerIdPolicy>(),
                              &mapper);

    auto peer = registry.acquire(Endpoint{2, 10});
    mapper.failMap = true;

    EXPECT_EQ(registry.acquire(Endpoint{2, 10}), peer);
}

TEST(EndpointRegistryTest, MapperFailureLeavesNothingRegistered)
{
    FakeMapper mapper;
    mapper.failMap = true;
    EndpointRegistry registry(std::make_unique<LowestFreePeerIdPolicy>(),
                              &mapper);

    EXPECT_EQ(registry.acquire(Endpoint{2, 10}), std::nullopt);
    EXPECT_EQ(registry.peerOf(Endpoint{2, 10}), std::nullopt);

    mapper.failMap = false;
    EXPECT_TRUE(registry.acquire(Endpoint{2, 10}));
}

namespace
{

/** @brief Mapper whose unmap() fails, like a transport that lost the entry */
class UnmapFailsMapper : public AddressMapper
{
  public:
    int map(PeerId, const Endpoint&) override
    {
        return 0;
    }

    int unmap(PeerId) override
    {
        return -1;
    }
};

} // namespace

TEST(EndpointRegistryTest, ReleaseForgetsTheEndpointWhenUnmapFails)
{
    UnmapFailsMapper mapper;
    EndpointRegistry registry(std::make_unique<IdentityPeerIdPolicy>(),
                              &mapper);
    auto peer = registry.acquire(Endpoint{2, 10});
    ASSERT_TRUE(peer);

    registry.release(Endpoint{2, 10});

    // The PeerId is free again even though the transport kept its mapping
    EXPECT_EQ(registry.peerOf(Endpoint{2, 10}), std::nullopt);
    EXPECT_EQ(registry.endpointOf(*peer), std::nullopt);
    EXPECT_EQ(registry.acquire(Endpoint{3, 10}), peer);
}

TEST(EndpointRegistryTest, PooledUsesEidAsPeerIdWhenFree)
{
    EndpointRegistry registry(std::make_unique<PooledPeerIdPolicy>());

    EXPECT_EQ(registry.acquire(Endpoint{1, 10}), 10);
    EXPECT_EQ(registry.acquire(Endpoint{1, 11}), 11);
    EXPECT_EQ(registry.acquire(Endpoint{2, 12}), 12);
}

TEST(EndpointRegistryTest, PooledGivesTheSameEidOnAnotherNetworkAnotherPeerId)
{
    EndpointRegistry registry(std::make_unique<PooledPeerIdPolicy>());

    EXPECT_EQ(registry.acquire(Endpoint{1, 10}), 10);
    EXPECT_EQ(registry.acquire(Endpoint{2, 10}), 254);
    EXPECT_EQ(registry.acquire(Endpoint{3, 10}), 253);
    EXPECT_EQ(registry.endpointOf(254), (Endpoint{2, 10}));
    EXPECT_EQ(registry.endpointOf(10), (Endpoint{1, 10}));
}

TEST(EndpointRegistryTest, PooledPeerIdDoesNotTakeTheEidOfALaterEndpoint)
{
    EndpointRegistry registry(std::make_unique<PooledPeerIdPolicy>());

    registry.acquire(Endpoint{1, 10});
    auto second = registry.acquire(Endpoint{2, 10});
    ASSERT_TRUE(second);

    // An endpoint that comes later still gets its own EID
    EXPECT_EQ(registry.acquire(Endpoint{1, 8}), 8);
    EXPECT_NE(*second, 8);
}

TEST(EndpointRegistryTest, PooledRejectsReservedEids)
{
    EndpointRegistry registry(std::make_unique<PooledPeerIdPolicy>());

    EXPECT_EQ(registry.acquire(Endpoint{1, 0}), std::nullopt);
    EXPECT_EQ(registry.acquire(Endpoint{1, 7}), std::nullopt);
    EXPECT_EQ(registry.acquire(Endpoint{1, 255}), std::nullopt);
}

TEST(EndpointRegistryTest, PooledRunsOutOfPeerIds)
{
    EndpointRegistry registry(std::make_unique<PooledPeerIdPolicy>());

    // Fill every PeerId from 8 to 254 with a distinct endpoint on Network 1
    for (unsigned endpointId = 8; endpointId <= 254; endpointId++)
    {
        ASSERT_TRUE(
            registry.acquire(Endpoint{1, static_cast<eid>(endpointId)}));
    }

    EXPECT_EQ(registry.acquire(Endpoint{2, 10}), std::nullopt);

    // Releasing one makes room again
    registry.release(Endpoint{1, 100});
    EXPECT_EQ(registry.acquire(Endpoint{2, 10}), 100);
}

TEST(EndpointRegistryTest, PooledReusesAReleasedPeerId)
{
    FakeMapper mapper;
    EndpointRegistry registry(std::make_unique<PooledPeerIdPolicy>(), &mapper);

    registry.acquire(Endpoint{1, 10});
    auto second = registry.acquire(Endpoint{2, 10});
    ASSERT_TRUE(second);
    EXPECT_EQ(mapper.mapped.at(*second), (Endpoint{2, 10}));

    registry.release(Endpoint{2, 10});
    EXPECT_FALSE(mapper.mapped.contains(*second));
    EXPECT_EQ(registry.acquire(Endpoint{3, 10}), second);
}
