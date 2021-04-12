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
        {"/xyz/openbmc_project/inventory/system0/chassis0/io_board0/"
         "powersupply0",
         {120, 1, 1}},
        {"/xyz/openbmc_project/inventory/system0/chassis0/io_board1/"
         "powersupply1",
         {120, 2, 1}},
        {"/xyz/openbmc_project/inventory/system0/chassis1/motherboard0/dimm0",
         {66, 1, 1}},
        {"/xyz/openbmc_project/inventory/system0/chassis1/motherboard1/dimm1",
         {66, 2, 1}},
        {"/xyz/openbmc_project/inventory/system0/chassis0/motherboard0/dimm0",
         {142, 1, 1}},
        {"/xyz/openbmc_project/inventory/system0/chassis1/motherboard1/dimm1",
         {142, 2, 1}},
        {"/xyz/openbmc_project/inventory/system0/chassis0/motherboard0/cpu0/"
         "core0",
         {32903, 1, 1}},
        {"/xyz/openbmc_project/inventory/system0/chassis0/motherboard0/cpu1/"
         "core1",
         {32903, 2, 1}}};

    const EntityAssociations entityAssociations = {
        {{45, 1, 1}, {60, 1, 1}, {60, 2, 1}, {64, 1, 1}},
        {{60, 1, 1}, {120, 1, 1}},
        {{60, 2, 1}, {120, 2, 1}},
        {{45, 2, 1}, {64, 2, 1}},
        {{64, 1, 1}, {142, 1, 1}, {135, 1, 1}, {135, 2, 1}},
        {{64, 2, 1}, {142, 2, 1}},
        {{135, 1, 1}, {32903, 1, 1}},
        {{135, 2, 1}, {32903, 2, 1}}};

    pldm_entity entities[15]{};

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
    entities[11].entity_type = 135;
    entities[12].entity_type = 135;
    entities[13].entity_type = 32903;
    entities[14].entity_type = 32903;

    auto tree = pldm_entity_association_tree_init();

    auto l1 = pldm_entity_association_tree_add(tree, &entities[0], nullptr,
                                               PLDM_ENTITY_ASSOCIAION_PHYSICAL);

    auto l2a = pldm_entity_association_tree_add(
        tree, &entities[1], l1, PLDM_ENTITY_ASSOCIAION_PHYSICAL);
    auto l2b = pldm_entity_association_tree_add(
        tree, &entities[2], l1, PLDM_ENTITY_ASSOCIAION_PHYSICAL);

    auto l3a = pldm_entity_association_tree_add(
        tree, &entities[3], l2a, PLDM_ENTITY_ASSOCIAION_PHYSICAL);
    auto l3b = pldm_entity_association_tree_add(
        tree, &entities[4], l2a, PLDM_ENTITY_ASSOCIAION_PHYSICAL);

    pldm_entity_association_tree_add(tree, &entities[5], l3a,
                                     PLDM_ENTITY_ASSOCIAION_PHYSICAL);
    pldm_entity_association_tree_add(tree, &entities[6], l3b,
                                     PLDM_ENTITY_ASSOCIAION_PHYSICAL);

    auto l5a = pldm_entity_association_tree_add(
        tree, &entities[7], l2b, PLDM_ENTITY_ASSOCIAION_PHYSICAL);
    auto l5b = pldm_entity_association_tree_add(
        tree, &entities[8], l2b, PLDM_ENTITY_ASSOCIAION_PHYSICAL);

    pldm_entity_association_tree_add(tree, &entities[9], l5a,
                                     PLDM_ENTITY_ASSOCIAION_PHYSICAL);
    pldm_entity_association_tree_add(tree, &entities[10], l5b,
                                     PLDM_ENTITY_ASSOCIAION_PHYSICAL);
    auto l6a = pldm_entity_association_tree_add(
        tree, &entities[11], l5a, PLDM_ENTITY_ASSOCIAION_PHYSICAL);
    auto l6b = pldm_entity_association_tree_add(
        tree, &entities[12], l5b, PLDM_ENTITY_ASSOCIAION_PHYSICAL);
    pldm_entity_association_tree_add(tree, &entities[13], l6a,
                                     PLDM_ENTITY_ASSOCIAION_LOGICAL);
    pldm_entity_association_tree_add(tree, &entities[14], l6b,
                                     PLDM_ENTITY_ASSOCIAION_LOGICAL);

    ObjectPathMaps objPathMap;
    updateEntityAssociation(entityAssociations, tree, objPathMap);

    EXPECT_EQ(objPathMap.size(), 4);
    EXPECT_EQ(retObjectMaps.size(), 7);

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
