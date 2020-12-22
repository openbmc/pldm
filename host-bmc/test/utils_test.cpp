#include "../utils.hpp"

#include <filesystem>

#include <gtest/gtest.h>

namespace fs = std::filesystem;
using namespace pldm;
using namespace pldm::hostbmc::utils;

TEST(EntityAssociation, addObjectPathEntityAssociationMap)
{
    /* name         type
     * system       1
     * io_board     2
     * chassis      3
     * ps0          4
     * ps1          5
     * mboard       6
     * dimm0        7
     * dimm1        8
     * ps2          9
     */

    ObjectPathMaps retObjectMaps = {
        {"/xyz/openbmc_project/system/chassis0/mboard0/dimm00", {7, 0, 1}},
        {"/xyz/openbmc_project/system/chassis0/mboard0/dimm10", {8, 0, 1}},
        {"/xyz/openbmc_project/system/io_board0/ps00", {4, 0, 1}},
        {"/xyz/openbmc_project/system/io_board0/ps10", {5, 0, 1}},
        {"/xyz/openbmc_project/system/io_board0/ps20", {9, 0, 1}}};

    EntityAssociationMaps entityAssociationMap = {
        {"system", {{"io_board", {2, 0, 1}}, {"chassis", {3, 0, 1}}}},
        {"io_board",
         {{"ps0", {4, 0, 1}}, {"ps1", {5, 0, 1}}, {"ps2", {9, 0, 1}}}},
        {"chassis", {{"mboard", {6, 0, 1}}}},
        {"mboard", {{"dimm0", {7, 0, 1}}, {"dimm1", {8, 0, 1}}}}};

    fs::path path{"/xyz/openbmc_project/system"};
    ObjectPathMaps objPathMap;

    addObjectPathEntityAssociationMap(entityAssociationMap,
                                      entityAssociationMap.at("system"), path,
                                      objPathMap);

    for (auto& obj : objPathMap)
    {
        for (auto& retObj : retObjectMaps)
        {
            if (obj.first == retObj.first)
            {
                ASSERT_EQ(obj.second.entity_type, retObj.second.entity_type);
                ASSERT_EQ(obj.second.entity_instance_num,
                          retObj.second.entity_instance_num);
                ASSERT_EQ(obj.second.entity_container_id,
                          retObj.second.entity_container_id);

                break;
            }
        }
    }
}