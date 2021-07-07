#include "libpldm/pdr.h"

#include "../utils.hpp"

#include <filesystem>

#include <gtest/gtest.h>

namespace fs = std::filesystem;
using namespace pldm;
using namespace pldm::hostbmc::utils;

// TEST(EntityAssociation, addObjectPathEntityAssociations1)
// {
//     ObjectPathMaps retObjectMaps = {
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu0/core5",
//          {32903, 5, 3}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu0/core6",
//          {32903, 6, 3}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu0/core8",
//          {32903, 8, 3}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu0/core9",
//          {32903, 9, 3}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu0/core11",
//          {32903, 11, 3}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu0/core12",
//          {32903, 12, 3}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu0/core13",
//          {32903, 13, 3}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu0/core14",
//          {32903, 14, 3}},

//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu1/core1",
//          {32903, 1, 4}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu1/core2",
//          {32903, 2, 4}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu1/core4",
//          {32903, 4, 4}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu1/core10",
//          {32903, 10, 4}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu1/core12",
//          {32903, 12, 4}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu1/core13",
//          {32903, 13, 4}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm0/"
//          "cpu1/core14",
//          {32903, 14, 4}},

//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu0/core0",
//          {32903, 0, 6}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu0/core2",
//          {32903, 2, 6}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu0/core3",
//          {32903, 3, 6}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu0/core5",
//          {32903, 5, 6}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu0/core6",
//          {32903, 6, 6}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu0/core7",
//          {32903, 7, 6}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu0/core8",
//          {32903, 8, 6}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu0/core9",
//          {32903, 9, 6}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu0/core15",
//          {32903, 15, 6}},

//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu1/core1",
//          {32903, 1, 7}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu1/core2",
//          {32903, 2, 7}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu1/core4",
//          {32903, 4, 7}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu1/core8",
//          {32903, 8, 7}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu1/core10",
//          {32903, 10, 7}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dcm1/"
//          "cpu1/core14",
//          {32903, 14, 7}},

//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dimm0",
//          {66, 0, 1}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dimm24",
//          {66, 24, 1}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dimm32",
//          {66, 32, 1}},
//         {"/xyz/openbmc_project/inventory/system1/chassis1/motherboard1/dimm56",
//          {66, 56, 1}}};

//     EntityAssociations entityAssociations = {
//         {{64, 1, 0},
//          {67, 0, 1},
//          {67, 1, 1},
//          {66, 0, 1},
//          {66, 24, 1},
//          {66, 32, 1},
//          {66, 56, 1}},
//         {{67, 0, 1}, {135, 0, 2}, {135, 1, 2}},
//         {{67, 1, 1}, {135, 0, 5}, {135, 1, 5}},
//         {{135, 0, 5},
//          {32903, 0, 6},
//          {32903, 2, 6},
//          {32903, 3, 6},
//          {32903, 5, 6},
//          {32903, 6, 6},
//          {32903, 7, 6},
//          {32903, 8, 6},
//          {32903, 9, 6},
//          {32903, 15, 6}},
//         {{135, 1, 5},
//          {32903, 1, 7},
//          {32903, 2, 7},
//          {32903, 4, 7},
//          {32903, 8, 7},
//          {32903, 10, 7},
//          {32903, 14, 7}},
//         {{135, 0, 2},
//          {32903, 5, 3},
//          {32903, 6, 3},
//          {32903, 8, 3},
//          {32903, 9, 3},
//          {32903, 11, 3},
//          {32903, 12, 3},
//          {32903, 13, 3},
//          {32903, 14, 3}},
//         {{135, 1, 2},
//          {32903, 1, 4},
//          {32903, 2, 4},
//          {32903, 4, 4},
//          {32903, 10, 4},
//          {32903, 12, 4},
//          {32903, 13, 4},
//          {32903, 14, 4}}};

//     pldm_entity entities[41]{};

//     entities[0].entity_type = 64;
//     entities[0].entity_container_id = 0;
//     entities[1].entity_type = 67;
//     entities[1].entity_container_id = 1;
//     entities[2].entity_type = 67;
//     entities[2].entity_container_id = 1;
//     entities[3].entity_type = 66;
//     entities[3].entity_container_id = 1;
//     entities[4].entity_type = 66;
//     entities[4].entity_container_id = 1;
//     entities[5].entity_type = 66;
//     entities[5].entity_container_id = 1;
//     entities[6].entity_type = 66;
//     entities[6].entity_container_id = 1;

//     entities[7].entity_type = 135;
//     entities[7].entity_container_id = 2;
//     entities[8].entity_type = 135;
//     entities[8].entity_container_id = 2;

//     entities[9].entity_type = 135;
//     entities[9].entity_container_id = 5;
//     entities[10].entity_type = 135;
//     entities[10].entity_container_id = 5;

//     entities[11].entity_type = 32903;
//     entities[11].entity_container_id = 6;
//     entities[12].entity_type = 32903;
//     entities[12].entity_container_id = 6;
//     entities[13].entity_type = 32903;
//     entities[13].entity_container_id = 6;
//     entities[14].entity_type = 32903;
//     entities[14].entity_container_id = 6;
//     entities[15].entity_type = 32903;
//     entities[15].entity_container_id = 6;
//     entities[16].entity_type = 32903;
//     entities[16].entity_container_id = 6;
//     entities[17].entity_type = 32903;
//     entities[17].entity_container_id = 6;
//     entities[18].entity_type = 32903;
//     entities[18].entity_container_id = 6;
//     entities[19].entity_type = 32903;
//     entities[19].entity_container_id = 6;

//     entities[20].entity_type = 32903;
//     entities[20].entity_container_id = 7;
//     entities[21].entity_type = 32903;
//     entities[21].entity_container_id = 7;
//     entities[22].entity_type = 32903;
//     entities[22].entity_container_id = 7;
//     entities[23].entity_type = 32903;
//     entities[23].entity_container_id = 7;
//     entities[24].entity_type = 32903;
//     entities[24].entity_container_id = 7;
//     entities[25].entity_type = 32903;
//     entities[25].entity_container_id = 7;

//     entities[26].entity_type = 32903;
//     entities[26].entity_container_id = 3;
//     entities[27].entity_type = 32903;
//     entities[27].entity_container_id = 3;
//     entities[28].entity_type = 32903;
//     entities[28].entity_container_id = 3;
//     entities[29].entity_type = 32903;
//     entities[29].entity_container_id = 3;
//     entities[30].entity_type = 32903;
//     entities[30].entity_container_id = 3;
//     entities[31].entity_type = 32903;
//     entities[31].entity_container_id = 3;
//     entities[32].entity_type = 32903;
//     entities[32].entity_container_id = 3;
//     entities[33].entity_type = 32903;
//     entities[33].entity_container_id = 3;

//     entities[34].entity_type = 32903;
//     entities[34].entity_container_id = 4;
//     entities[35].entity_type = 32903;
//     entities[35].entity_container_id = 4;
//     entities[36].entity_type = 32903;
//     entities[36].entity_container_id = 4;
//     entities[37].entity_type = 32903;
//     entities[37].entity_container_id = 4;
//     entities[38].entity_type = 32903;
//     entities[38].entity_container_id = 4;
//     entities[39].entity_type = 32903;
//     entities[39].entity_container_id = 4;
//     entities[40].entity_type = 32903;
//     entities[40].entity_container_id = 4;

//     auto tree = pldm_entity_association_tree_init();

//     auto l1 = pldm_entity_association_tree_add(
//         tree, &entities[0], 1, nullptr, PLDM_ENTITY_ASSOCIAION_PHYSICAL,
//         true);

//     auto l2a = pldm_entity_association_tree_add(
//         tree, &entities[1], 0, l1, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     auto l2b = pldm_entity_association_tree_add(
//         tree, &entities[2], 1, l1, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[3], 0, l1,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[4], 24, l1,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[5], 32, l1,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[6], 56, l1,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

//     auto l3a = pldm_entity_association_tree_add(
//         tree, &entities[7], 0, l2a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     auto l3b = pldm_entity_association_tree_add(
//         tree, &entities[8], 1, l2a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

//     auto l4a = pldm_entity_association_tree_add(
//         tree, &entities[9], 0, l2b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     auto l4b = pldm_entity_association_tree_add(
//         tree, &entities[10], 1, l2b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

//     pldm_entity_association_tree_add(tree, &entities[11], 0, l4a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[12], 2, l4a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[13], 3, l4a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[14], 5, l4a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[15], 6, l4a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[16], 7, l4a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[17], 8, l4a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[18], 9, l4a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[19], 15, l4a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

//     pldm_entity_association_tree_add(tree, &entities[20], 1, l4b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[21], 2, l4b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[22], 4, l4b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[23], 8, l4b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[24], 10, l4b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[25], 14, l4b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

//     pldm_entity_association_tree_add(tree, &entities[26], 5, l3a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[27], 6, l3a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[28], 8, l3a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[29], 9, l3a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[30], 11, l3a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[31], 12, l3a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[32], 13, l3a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[33], 14, l3a,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

//     pldm_entity_association_tree_add(tree, &entities[34], 1, l3b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[35], 2, l3b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[36], 4, l3b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[37], 10, l3b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[38], 12, l3b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[39], 13, l3b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
//     pldm_entity_association_tree_add(tree, &entities[40], 14, l3b,
//                                      PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

//     ObjectPathMaps objPathMap;
//     updateEntityAssociation(entityAssociations, tree, objPathMap);

//     EXPECT_EQ(objPathMap.size(), 34);
//     EXPECT_EQ(retObjectMaps.size(), 34);

//     for (auto& obj : objPathMap)
//     {
//         std::cerr << "george: obj first = " << obj.first << std::endl;
//         if (retObjectMaps.contains(obj.first))
//         {
//             EXPECT_EQ(obj.second.entity_type,
//                       retObjectMaps[obj.first].entity_type);
//             EXPECT_EQ(obj.second.entity_instance_num,
//                       retObjectMaps[obj.first].entity_instance_num);

//             uint16_t id = pldm_extract_host_container_id(tree, &obj.second);
//             if (id == 0xFFFF)
//             {
//                 std::cerr << "george: obj.second.entity_type = "
//                           << obj.second.entity_type << std::endl;
//                 std::cerr << "george: obj.second.entity_instance_num = "
//                           << obj.second.entity_instance_num << std::endl;
//                 std::cerr << "george: obj.second.entity_container_id = "
//                           << obj.second.entity_container_id << std::endl;
//             }
//             EXPECT_EQ(pldm_extract_host_container_id(tree, &obj.second),
//                       pldm_extract_host_container_id(
//                           tree, &retObjectMaps[obj.first]));
//         }
//     }

//     pldm_entity_association_tree_destroy(tree);
// }