#include "serialize.hpp"

#include "../common/utils.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>
#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

PHOSPHOR_LOG2_USING;

// Register class version with Cereal
CEREAL_CLASS_VERSION(pldm::serialize::Serialize, 1)

namespace pldm
{
namespace serialize
{
namespace fs = std::filesystem;

void Serialize::serialize(const std::string& path, const std::string& intf,
                          const std::string& name, utils::PropertyValue value)
{
    if (path.empty() || intf.empty())
    {
        error("Entity Path or Interface is Empty for serialization.");
        return;
    }

    if (!entityPathMaps.contains(path))
    {
        error("Entity '{PATH}' Does not Exist in entity map for serialization",
              "PATH", path);
        return;
    }
    auto entity = entityPathMaps[path];
    uint16_t type = entity.entity_type;
    uint16_t num = entity.entity_instance_num;
    uint16_t cid = entity.entity_container_id;

    if (!savedObjs.contains(type) || !savedObjs[type].contains(path))
    {
        std::map<std::string, std::map<std::string, pldm::utils::PropertyValue>>
            maps{{{intf, {{name, value}}}}};
        savedObjs[type][path] = std::make_tuple(num, cid, maps);
    }
    else
    {
        auto& [num, cid, objs] = savedObjs[type][path];

        if (!objs.empty() && value == objs[intf][name])
        {
            // The value in memory cache is same as the new value
            // so no need to serialise it again
            return;
        }

        // The value is changed and is not equal to
        // the value in the in-memory cache, so update it
        // and update the persistent cache file
        objs[intf][name] = value;
    }

    if (!storeEntityTypes.contains(entity.entity_type))
    {
        return;
    }

    auto dir = filePath.parent_path();
    if (!fs::exists(dir))
    {
        fs::create_directories(dir);
    }

    std::ofstream os(filePath.c_str(), std::ios::binary);
    cereal::BinaryOutputArchive oarchive(os);
    oarchive(savedObjs);
}

bool Serialize::deserialize()
{
    try
    {
        if (!fs::exists(filePath))
        {
            error("File '{PATH}' does not exist.", "PATH", filePath);
            return false;
        }
        savedObjs.clear();
        std::ifstream is(filePath.c_str(), std::ios::in | std::ios::binary);
        cereal::BinaryInputArchive iarchive(is);
        iarchive(savedObjs);

        return true;
    }
    catch (const cereal::Exception& e)
    {
        error("Failed to restore groups: {ERROR}", "ERROR", e);
        fs::remove(filePath);
    }

    return false;
}

void Serialize::setEntityTypes(const std::set<uint16_t>& storeEntities)
{
    storeEntityTypes = storeEntities;
}

void Serialize::setObjectPathMaps(const ObjectPathMaps& maps)
{
    for (const auto& [objpath, entity] : maps)
    {
        entityPathMaps.emplace(objpath, entity);
    }
}

void Serialize::reSerialize(const std::vector<uint16_t> types)
{
    if (types.empty())
    {
        return;
    }

    for (const auto& type : types)
    {
        if (savedObjs.contains(type))
        {
            savedObjs.erase(savedObjs.find(type));
        }
    }

    auto dir = filePath.parent_path();
    if (!fs::exists(dir))
    {
        fs::create_directories(dir);
    }

    std::ofstream os(filePath.c_str(), std::ios::binary);
    cereal::BinaryOutputArchive oarchive(os);
    oarchive(this->savedObjs);
}

} // namespace serialize
} // namespace pldm
