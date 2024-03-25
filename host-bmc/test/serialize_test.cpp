#include "../dbus/serialize.hpp"
#include "../dbus/type.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

#include <gtest/gtest.h>

using namespace pldm::dbus;
using namespace pldm::serialize;
using ObjectPathMaps = std::map<ObjectPath, pldm_entity_node*>;

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
    std::string path = "/xyz/openbmc_project/pldm";
    std::string interface = "com.ibm.PLDM.TEST";
    std::string propertyname = "TEST";
    std::string propValue = "unittest";
    objectMaps[path] = l1;

    // Changing file location to store data for testing
    pldm::serialize::Serialize::getSerialize().setfilePathForUnitTest(filepath);
    pldm::serialize::Serialize::getSerialize().setEntityTypes(entitytypes);
    pldm::serialize::Serialize::getSerialize().setObjectPathMaps(objectMaps);

    // Storing data as serialize way into file
    pldm::serialize::Serialize::getSerialize().serialize(
        path, interface, propertyname, propValue);

    pldm::dbus::SavedObjs savedObjs;
    std::map<std::string, pldm::dbus::PropertyValue> savedKeyVal;

    // Reading data as from file to verify
    std::ifstream is(filepath.c_str(), std::ios::in | std::ios::binary);
    cereal::BinaryInputArchive iarchive(is);
    iarchive(savedObjs, savedKeyVal);

    uint16_t type = entities.entity_type, instancenum, cid;
    std::map<std::string, std::map<std::string, pldm::dbus::PropertyValue>>
        maps;
    std::string intf, name;
    pldm::dbus::PropertyValue value;

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

    pldm_entity_association_tree_destroy(tree);
}

TEST(Serialize, DeserializeBadPath)
{
    pldm::serialize::Serialize::getSerialize().setfilePathForUnitTest(
        PERSISTENT_FILE);
    bool value = pldm::serialize::Serialize::getSerialize().deserialize();
    EXPECT_EQ(value, false);

    // Changin file location which does not present so function should return
    // false.
    std::string filepath = "/tmp/temp1";

    // Changing file location to store data for testing
    pldm::serialize::Serialize::getSerialize().setfilePathForUnitTest(filepath);
    bool value1 = pldm::serialize::Serialize::getSerialize().deserialize();
    EXPECT_EQ(value1, false);
}
