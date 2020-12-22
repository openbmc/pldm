#include "../utils.hpp"

#include <filesystem>

#include <gtest/gtest.h>

namespace fs = std::filesystem;
using namespace pldm;
using namespace pldm::hostbmc::utils;

TEST(EntityAssociation, addObjectPathEntityAssociations)
{
    ObjectPathMaps retObjectMaps = {
        {"/xyz/openbmc_project/inventory/system/chassis0/io_board0/"
         "powersupply0",
         {120, 1, 1}},
        {"/xyz/openbmc_project/inventory/system/chassis0/io_board1/"
         "powersupply1",
         {120, 2, 1}},
        {"/xyz/openbmc_project/inventory/system/chassis1/motherboard0/dimm0",
         {66, 1, 1}},
        {"/xyz/openbmc_project/inventory/system/chassis1/motherboard1/dimm1",
         {66, 2, 1}}};

    const EntityAssociations entityAssociations = {
        {{45, 1, 1}, {60, 1, 1}, {60, 2, 1}},
        {{60, 1, 1}, {120, 1, 1}},
        {{60, 2, 1}, {120, 2, 1}},
        {{45, 2, 1}, {64, 1, 1}, {64, 2, 1}},
        {{64, 1, 1}, {66, 1, 1}},
        {{64, 2, 1}, {66, 2, 1}}};

    fs::path path{"/xyz/openbmc_project/inventory/system"};
    ObjectPathMaps objPathMap;

    Entities parentsEntity = getParentEntites(entityAssociations);
    for (auto& p : parentsEntity)
    {
        addObjectPathEntityAssociations(entityAssociations, p, path,
                                        objPathMap);
    }

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
}