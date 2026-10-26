#include "common/endpoint.hpp"

#include <gtest/gtest.h>

using namespace pldm;

TEST(EndpointTest, ToEndpointTakesNetworkAndEid)
{
    MctpInfo info{10, "uuid", "", 3, std::nullopt};

    auto endpoint = toEndpoint(info);

    EXPECT_EQ(endpoint.networkId, 3u);
    EXPECT_EQ(endpoint.endpointId, 10);
}

TEST(EndpointTest, SameEidOnDifferentNetworksIsDistinct)
{
    Endpoint a{1, 10};
    Endpoint b{2, 10};

    EXPECT_NE(a, b);
    EXPECT_LT(a, b);
    EXPECT_EQ(a, (Endpoint{1, 10}));
}

TEST(EndpointTest, FormatIncludesNetworkAndEid)
{
    EXPECT_EQ(formatEndpoint(Endpoint{4, 255}), "Network 4 EID 255");
}
