#include "../dbus/serialize.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

#include <gtest/gtest.h>

using namespace std;
using namespace pldm::serialize;

TEST(Serialize, SerializeGoodPath)
{
    // Setting up basic requirement for function to store data
    std::string filepath = "/tmp/temp";
    pldm_entity entities;
    entities.entity_type = 1;
    entities.entity_instance_num = 2;
    entities.entity_container_id = 3;
    auto tree = pldm_entity_association_tree_init();
    auto l1 = pldm_entity_association_tree_add(tree, &entities, 0xFFFF, nullptr,
                                               PLDM_ENTITY_ASSOCIAION_PHYSICAL);
    EXPECT_NE(l1, nullptr);
    std::set<uint16_t> entitytypes{entities.entity_type};
    ObjectPathMaps objectMaps;
    std::string path = "/xyz/local_project/DEF";
    std::string interface = "com.abc.DEF.TEST";
    std::string propertyname = "TEST";
    std::string propValue = "unittest";
    objectMaps[path] = l1;

    // Changing file location to store data for testing
    pldm::serialize::Serialize::getSerialize(filepath);
    pldm::serialize::Serialize::getSerialize(filepath).setEntityTypes(
        entitytypes);
    pldm::serialize::Serialize::getSerialize(filepath).setObjectPathMaps(
        objectMaps);

    // Storing data as serialize way into file
    pldm::serialize::Serialize::getSerialize(filepath).serialize(
        path, interface, propertyname, propValue);

    pldm::utils::SavedObjs savedObjs;

    // Reading data as from file to verify
    std::ifstream is(filepath.c_str(), std::ios::in | std::ios::binary);
    cereal::BinaryInputArchive iarchive(is);
    iarchive(savedObjs);

    uint16_t type = entities.entity_type, instancenum, cid;
    std::map<std::string, std::map<std::string, pldm::utils::PropertyValue>>
        maps;
    std::string intf, name;
    pldm::utils::PropertyValue value;

    // Verifying data from original data
    EXPECT_EQ(true, savedObjs.contains(type));
    EXPECT_EQ(true, savedObjs[type].contains(path));
    auto tuple = savedObjs[type][path];
    std::tie(instancenum, cid, maps) = tuple;
    auto insideMap = maps[interface];

    EXPECT_EQ(true, insideMap.contains(propertyname));
    EXPECT_EQ(propValue, std::get<std::string>(insideMap[propertyname]));
    EXPECT_EQ(instancenum, entities.entity_instance_num);
    EXPECT_EQ(cid, entities.entity_container_id);

    // File path "/tmp/temp" not present at first time,
    // testcase created it, so function should return true.
    bool value1 =
        pldm::serialize::Serialize::getSerialize(filepath).deserialize();
    EXPECT_EQ(value1, true);

    pldm_entity_association_tree_destroy(tree);
}

TEST(Serialize, DeserializeGoodPath)
{
    // Changing file location which is present but testcase doesn't have access
    // permission since it's internal system location, so function should return
    // false.
    bool value = pldm::serialize::Serialize::getSerialize().deserialize();
    EXPECT_EQ(value, false);
}

TEST(Serialize, DeserializeGoodPath1)
{
    std::string filepath = "/tmp/temp";
    pldm::serialize::Serialize::getSerialize(filepath);
}

TEST(Serialize, DeserializeBadPath)
{
    // Changing file location which does not present, so function should return
    // false.
    std::string filepath = "/tmp/temp1";

    bool value1 =
        pldm::serialize::Serialize::getSerialize(filepath).deserialize();
    EXPECT_EQ(value1, false);
}
