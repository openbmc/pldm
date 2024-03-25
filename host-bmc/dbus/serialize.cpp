#include "serialize.hpp"

#include "../utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <fstream>

PHOSPHOR_LOG2_USING;

using namespace pldm::hostbmc::utils;

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
        error("Entity Path or Interface is Empty for serialization");
        return;
    }

    if (!entityPathMaps.contains(path))
    {
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

        if (objs.empty())
        {
            objs[intf][name] = value;
        }
        else
        {
            if (value != objs[intf][name])
            {
                // The value is changed and is not equal to
                // the value in the in-memory cache, so update it
                // and update the persistent cache file
                objs[intf][name] = value;
            }
            else
            {
                // The value in memory cache is same as the new value
                // so no need to serialise it again
                return;
            }
        }
    }

    if (!storeEntityTypes.contains(entityPathMaps[path].entity_type))
    {
        return;
    }

    auto dir = filePath.parent_path();
    if (!fs::exists(dir))
    {
        fs::create_directories(dir);
    }

    std::ofstream file(filePath.c_str(), std::ios::out);
    json savedObjsjson = pldm::hostbmc::utils::ToJSON(savedObjs);
    file << savedObjsjson.dump(4);
    file.close();
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
        std::ifstream file(filePath.c_str(), std::ios::in);
        json savedjsonObj = json::parse(file);
        savedObjs = pldm::hostbmc::utils::FromJSON(savedjsonObj);
        file.close();
        return true;
    }
    catch (const std::exception& e)
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
    for (const auto& [objpath, nodeentity] : maps)
    {
        pldm_entity entity = pldm_entity_extract(nodeentity);
        entityPathMaps.emplace(objpath, entity);
    }
}

void Serialize::reSerialize(const std::vector<uint16_t>& types)
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
    std::ofstream file(filePath.c_str(), std::ios::out);
    json savedObjsjson = pldm::hostbmc::utils::ToJSON(savedObjs);
    file << savedObjsjson.dump(4);
    file.close();
}

} // namespace serialize
} // namespace pldm
