#include "libpldm/pdr.h"

#include "../utils.hpp"

#include <filesystem>

#include <gtest/gtest.h>

namespace fs = std::filesystem;
using namespace pldm;
using namespace pldm::hostbmc::utils;

TEST(EntityAssociation, addObjectPathEntityAssociations1)
{

    pldm_entity entities[41]{};

    entities[0].entity_type = 64;
    entities[0].entity_container_id = 0;
    entities[1].entity_type = 67;
    entities[1].entity_container_id = 1;
    entities[2].entity_type = 67;
    entities[2].entity_container_id = 1;
    entities[3].entity_type = 66;
    entities[3].entity_container_id = 1;
    entities[4].entity_type = 66;
    entities[4].entity_container_id = 1;
    entities[5].entity_type = 66;
    entities[5].entity_container_id = 1;
    entities[6].entity_type = 66;
    entities[6].entity_container_id = 1;

    entities[7].entity_type = 135;
    entities[7].entity_container_id = 2;
    entities[8].entity_type = 135;
    entities[8].entity_container_id = 2;

    entities[9].entity_type = 135;
    entities[9].entity_container_id = 5;
    entities[10].entity_type = 135;
    entities[10].entity_container_id = 5;

    entities[11].entity_type = 32903;
    entities[11].entity_container_id = 6;
    entities[12].entity_type = 32903;
    entities[12].entity_container_id = 6;
    entities[13].entity_type = 32903;
    entities[13].entity_container_id = 6;
    entities[14].entity_type = 32903;
    entities[14].entity_container_id = 6;
    entities[15].entity_type = 32903;
    entities[15].entity_container_id = 6;
    entities[16].entity_type = 32903;
    entities[16].entity_container_id = 6;
    entities[17].entity_type = 32903;
    entities[17].entity_container_id = 6;
    entities[18].entity_type = 32903;
    entities[18].entity_container_id = 6;
    entities[19].entity_type = 32903;
    entities[19].entity_container_id = 6;

    entities[20].entity_type = 32903;
    entities[20].entity_container_id = 7;
    entities[21].entity_type = 32903;
    entities[21].entity_container_id = 7;
    entities[22].entity_type = 32903;
    entities[22].entity_container_id = 7;
    entities[23].entity_type = 32903;
    entities[23].entity_container_id = 7;
    entities[24].entity_type = 32903;
    entities[24].entity_container_id = 7;
    entities[25].entity_type = 32903;
    entities[25].entity_container_id = 7;

    entities[26].entity_type = 32903;
    entities[26].entity_container_id = 3;
    entities[27].entity_type = 32903;
    entities[27].entity_container_id = 3;
    entities[28].entity_type = 32903;
    entities[28].entity_container_id = 3;
    entities[29].entity_type = 32903;
    entities[29].entity_container_id = 3;
    entities[30].entity_type = 32903;
    entities[30].entity_container_id = 3;
    entities[31].entity_type = 32903;
    entities[31].entity_container_id = 3;
    entities[32].entity_type = 32903;
    entities[32].entity_container_id = 3;
    entities[33].entity_type = 32903;
    entities[33].entity_container_id = 3;

    entities[34].entity_type = 32903;
    entities[34].entity_container_id = 4;
    entities[35].entity_type = 32903;
    entities[35].entity_container_id = 4;
    entities[36].entity_type = 32903;
    entities[36].entity_container_id = 4;
    entities[37].entity_type = 32903;
    entities[37].entity_container_id = 4;
    entities[38].entity_type = 32903;
    entities[38].entity_container_id = 4;
    entities[39].entity_type = 32903;
    entities[39].entity_container_id = 4;
    entities[40].entity_type = 32903;
    entities[40].entity_container_id = 4;

    auto tree = pldm_entity_association_tree_init();

    auto l1 = pldm_entity_association_tree_add(
        tree, &entities[0], 1, nullptr, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

    auto l2a = pldm_entity_association_tree_add(
        tree, &entities[1], 0, l1, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l2b = pldm_entity_association_tree_add(
        tree, &entities[2], 1, l1, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

    auto l3a = pldm_entity_association_tree_add(
        tree, &entities[7], 0, l2a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l3b = pldm_entity_association_tree_add(
        tree, &entities[8], 1, l2a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

    auto l4a = pldm_entity_association_tree_add(
        tree, &entities[9], 0, l2b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l4b = pldm_entity_association_tree_add(
        tree, &entities[10], 1, l2b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

    auto l5a = pldm_entity_association_tree_add(
        tree, &entities[11], 0, l4a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l5b = pldm_entity_association_tree_add(
        tree, &entities[12], 2, l4a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l5c = pldm_entity_association_tree_add(
        tree, &entities[13], 3, l4a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l5d = pldm_entity_association_tree_add(
        tree, &entities[14], 5, l4a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l5e = pldm_entity_association_tree_add(
        tree, &entities[15], 6, l4a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l5f = pldm_entity_association_tree_add(
        tree, &entities[16], 7, l4a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l5g = pldm_entity_association_tree_add(
        tree, &entities[17], 8, l4a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l5h = pldm_entity_association_tree_add(
        tree, &entities[18], 9, l4a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l5i = pldm_entity_association_tree_add(
        tree, &entities[19], 15, l4a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

    auto l6a = pldm_entity_association_tree_add(
        tree, &entities[20], 1, l4b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l6b = pldm_entity_association_tree_add(
        tree, &entities[21], 2, l4b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l6c = pldm_entity_association_tree_add(
        tree, &entities[22], 4, l4b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l6d = pldm_entity_association_tree_add(
        tree, &entities[23], 8, l4b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l6e = pldm_entity_association_tree_add(
        tree, &entities[24], 10, l4b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l6f = pldm_entity_association_tree_add(
        tree, &entities[25], 14, l4b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

    auto l7a = pldm_entity_association_tree_add(
        tree, &entities[26], 5, l3a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l7b = pldm_entity_association_tree_add(
        tree, &entities[27], 6, l3a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l7c = pldm_entity_association_tree_add(
        tree, &entities[28], 8, l3a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l7d = pldm_entity_association_tree_add(
        tree, &entities[29], 9, l3a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l7e = pldm_entity_association_tree_add(
        tree, &entities[30], 11, l3a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l7f = pldm_entity_association_tree_add(
        tree, &entities[31], 12, l3a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l7g = pldm_entity_association_tree_add(
        tree, &entities[32], 13, l3a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l7h = pldm_entity_association_tree_add(
        tree, &entities[33], 14, l3a, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

    auto l8a = pldm_entity_association_tree_add(
        tree, &entities[34], 1, l3b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l8b = pldm_entity_association_tree_add(
        tree, &entities[35], 2, l3b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l8c = pldm_entity_association_tree_add(
        tree, &entities[36], 4, l3b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l8d = pldm_entity_association_tree_add(
        tree, &entities[37], 10, l3b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l8e = pldm_entity_association_tree_add(
        tree, &entities[38], 12, l3b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l8f = pldm_entity_association_tree_add(
        tree, &entities[39], 13, l3b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);
    auto l8g = pldm_entity_association_tree_add(
        tree, &entities[40], 14, l3b, PLDM_ENTITY_ASSOCIAION_PHYSICAL, true);

    EntityAssociations entityAssociations = {
        {l1, l2a, l2b},
        {l2a, l3a, l3b},
        {l2b, l4a, l4b},
        {l4a, l5a, l5b, l5c, l5d, l5e, l5f, l5g, l5h, l5i},
        {l4b, l6a, l6b, l6c, l6d, l6e, l6f},
        {l3a, l7a, l7b, l7c, l7d, l7e, l7f, l7g, l7h},
        {l3b, l8a, l8b, l8c, l8d, l8e, l8f, l8g}};

    ObjectPathMaps retObjectMaps = {
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu0/core5", l7a},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu0/core6", l7b},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu0/core8", l7c},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu0/core9", l7d},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu0/core11", l7e},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu0/core12", l7f},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu0/core13", l7g},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu0/core14", l7h},

        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu1/core1", l8a},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu1/core2", l8b},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu1/core4", l8c},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu1/core10", l8d},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu1/core12", l8e},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu1/core13", l8f},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm0/cpu1/core14", l8g},

        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu0/core0", l5a},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu0/core2", l5b},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu0/core3", l5c},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu0/core5", l5d},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu0/core6", l5e},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu0/core7", l5f},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu0/core8", l5g},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu0/core9", l5h},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu0/core15", l5i},

        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu1/core1", l6a},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu1/core2", l6b},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu1/core4", l6c},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu1/core8", l6d},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu1/core10", l6e},
        {"/xyz/openbmc_project/inventory/motherboard1/dcm1/cpu1/core14", l6f}};

    ObjectPathMaps objPathMap;
    updateEntityAssociation(entityAssociations, tree, objPathMap);

    EXPECT_EQ(objPathMap.size(), retObjectMaps.size());

    int index = 0;
    for (auto& obj : objPathMap)
    {
        if (retObjectMaps.contains(obj.first))
        {
            index++;
            pldm_entity entity = pldm_entity_extract(obj.second);
            pldm_entity retEntity =
                pldm_entity_extract(retObjectMaps[obj.first]);
            EXPECT_EQ(entity.entity_type, retEntity.entity_type);
            EXPECT_EQ(entity.entity_instance_num,
                      retEntity.entity_instance_num);
            EXPECT_EQ(pldm_extract_host_container_id(obj.second),
                      pldm_extract_host_container_id(retObjectMaps[obj.first]));
        }
    }
    EXPECT_EQ(index, retObjectMaps.size());
    pldm_entity_association_tree_destroy(tree);
}