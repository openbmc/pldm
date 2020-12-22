#include "libpldm/pdr.h"

#include "../utils.hpp"

#include <filesystem>

#include <gtest/gtest.h>

namespace fs = std::filesystem;
using namespace pldm;
using namespace pldm::hostbmc::utils;

TEST(EntityAssociation, addObjectPathEntityAssociations)
{
    ObjectPathMaps retObjectMaps = {
        {"/xyz/openbmc_project/inventory/system1/chassis1/io_board1/"
         "powersupply1",
         {120, 1, 1}},
        {"/xyz/openbmc_project/inventory/system1/chassis1/io_board2/"
         "powersupply2",
         {120, 2, 1}},
        {"/xyz/openbmc_project/inventory/system1/chassis2/motherboard1/dimm1",
         {66, 1, 1}},
        {"/xyz/openbmc_project/inventory/system1/chassis2/motherboard2/dimm2",
         {66, 2, 1}}};

    const EntityAssociations entityAssociations = {
        {{45, 1, 1}, {60, 1, 1}, {60, 2, 1}},
        {{60, 1, 1}, {120, 1, 1}},
        {{60, 2, 1}, {120, 2, 1}},
        {{45, 2, 1}, {64, 1, 1}, {64, 2, 1}},
        {{64, 1, 1}, {66, 1, 1}},
        {{64, 2, 1}, {66, 2, 1}}};

    pldm_entity entities[11]{};

    entities[0].entity_type = 11521;
    entities[1].entity_type = 45;
    entities[2].entity_type = 45;
    entities[3].entity_type = 60;
    entities[4].entity_type = 60;
    entities[5].entity_type = 120;
    entities[6].entity_type = 120;
    entities[7].entity_type = 64;
    entities[8].entity_type = 64;
    entities[9].entity_type = 66;
    entities[10].entity_type = 66;

    auto tree = pldm_entity_association_tree_init();

    auto l1 = pldm_entity_association_tree_add(
        tree, &entities[0], 0xFFFF, nullptr, PLDM_ENTITY_ASSOCIAION_PHYSICAL,
        false);

    auto l2a = pldm_entity_association_tree_add(
        tree, &entities[1], 0xFFFF, l1, PLDM_ENTITY_ASSOCIAION_PHYSICAL, false);
    auto l2b = pldm_entity_association_tree_add(
        tree, &entities[2], 0xFFFF, l1, PLDM_ENTITY_ASSOCIAION_PHYSICAL, false);

    auto l3a = pldm_entity_association_tree_add(tree, &entities[3], 0xFFFF, l2a,
                                                PLDM_ENTITY_ASSOCIAION_PHYSICAL,
                                                false);
    auto l3b = pldm_entity_association_tree_add(tree, &entities[4], 0xFFFF, l2a,
                                                PLDM_ENTITY_ASSOCIAION_PHYSICAL,
                                                false);

    pldm_entity_association_tree_add(tree, &entities[5], 0xFFFF, l3a,
                                     PLDM_ENTITY_ASSOCIAION_PHYSICAL, false);
    pldm_entity_association_tree_add(tree, &entities[6], 0xFFFF, l3b,
                                     PLDM_ENTITY_ASSOCIAION_PHYSICAL, false);

    auto l5a = pldm_entity_association_tree_add(tree, &entities[7], 0xFFFF, l2b,
                                                PLDM_ENTITY_ASSOCIAION_PHYSICAL,
                                                false);
    auto l5b = pldm_entity_association_tree_add(tree, &entities[8], 0xFFFF, l2b,
                                                PLDM_ENTITY_ASSOCIAION_PHYSICAL,
                                                false);

    pldm_entity_association_tree_add(tree, &entities[9], 0xFFFF, l5a,
                                     PLDM_ENTITY_ASSOCIAION_PHYSICAL, false);
    pldm_entity_association_tree_add(tree, &entities[10], 0xFFFF, l5b,
                                     PLDM_ENTITY_ASSOCIAION_PHYSICAL, false);

    ObjectPathMaps objPathMap;
    updateEntityAssociation(entityAssociations, tree, objPathMap);

    EXPECT_EQ(objPathMap.size(), 4);
    EXPECT_EQ(retObjectMaps.size(), 4);

    for (auto& obj : objPathMap)
    {
        EXPECT_NE(retObjectMaps.find(obj.first), retObjectMaps.end());
        EXPECT_EQ(obj.second.entity_type, retObjectMaps[obj.first].entity_type);
        EXPECT_EQ(obj.second.entity_instance_num,
                  retObjectMaps[obj.first].entity_instance_num);
        EXPECT_EQ(obj.second.entity_container_id,
                  retObjectMaps[obj.first].entity_container_id);
    }

    pldm_entity_association_tree_destroy(tree);
}