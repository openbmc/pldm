#include "common/instance_id.hpp"
#include "host-bmc/host_pdr_handler.hpp"
#include "test/test_instance_id.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace pldm
{

class HostPDRHandlerTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        repo.reset(pldm_pdr_init());
        entityTree.reset(pldm_entity_association_tree_init());
        bmcEntityTree.reset(pldm_entity_association_tree_init());
        handler = std::make_unique<HostPDRHandler>(
            0, 8, event, repo.get(), "", entityTree.get(),
            bmcEntityTree.get(), instanceIdDb, nullptr,
            pldm::utils::EntityMaps{});
    }

    void seedTemporaryState()
    {
        handler->mergedHostPdrs = true;
        handler->pendingStateSensorPDRs.emplace_back(
            std::vector<uint8_t>{1});
        handler->pendingFruRecordSetPDRs.emplace_back(
            std::vector<uint8_t>{2});
    }

    void processFailedFetch()
    {
        handler->processHostPDRs(8, nullptr, 0);
    }

    bool temporaryStateIsEmpty() const
    {
        return !handler->mergedHostPdrs &&
               handler->pendingStateSensorPDRs.empty() &&
               handler->pendingFruRecordSetPDRs.empty();
    }

    std::unique_ptr<pldm_pdr, decltype(&pldm_pdr_destroy)> repo{
        nullptr, pldm_pdr_destroy};
    std::unique_ptr<pldm_entity_association_tree,
                    decltype(&pldm_entity_association_tree_destroy)>
        entityTree{nullptr, pldm_entity_association_tree_destroy};
    std::unique_ptr<pldm_entity_association_tree,
                    decltype(&pldm_entity_association_tree_destroy)>
        bmcEntityTree{nullptr, pldm_entity_association_tree_destroy};
    sdeventplus::Event event{sdeventplus::Event::get_default()};
    TestInstanceIdDb instanceIdDb;
    std::unique_ptr<HostPDRHandler> handler;
};

TEST_F(HostPDRHandlerTest, FailedFetchDoesNotLeakStateToNextFetch)
{
    seedTemporaryState();

    processFailedFetch();

    EXPECT_TRUE(temporaryStateIsEmpty());

    seedTemporaryState();

    PDRRecordHandles recordHandles;
    handler->fetchPDR(std::move(recordHandles));

    EXPECT_TRUE(temporaryStateIsEmpty());
}

} // namespace pldm
