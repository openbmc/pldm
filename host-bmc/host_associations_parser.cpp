#include "host_associations_parser.hpp"

#include <xyz/openbmc_project/Common/error.hpp>

#include <fstream>
#include <iostream>

using namespace pldm::utils;

namespace pldm
{
namespace host_associations
{
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr auto hostfruAssociationsJson = "host_fru_associations.json";

void HostAssociationsParser::parseHostAssociations(const std::string& jsonPath)
{
    fs::path jsonDir(jsonPath);
    if (!fs::exists(jsonDir) || fs::is_empty(jsonDir))
    {
        std::cerr << "Host Associations json path does not exist, DIR="
                  << jsonPath << "\n";
        return;
    }

    fs::path jsonFilePath = jsonDir / hostfruAssociationsJson;
    jsonFilePath = "/tmp/host_fru_associations.json";
    if (!fs::exists(jsonFilePath))
    {
        std::cerr << "json does not exist, PATH=" << jsonFilePath << "\n";
        throw InternalFailure();
    }

    std::ifstream jsonFile(jsonFilePath);
    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        std::cerr << "Parsing json file failed, FILE=" << jsonFilePath << "\n";
        throw InternalFailure();
    }
    const Json empty{};
    const std::vector<Json> emptyList{};

    auto entries = data.value("associations", emptyList);
    for (const auto& entry : entries)
    {
        entity p{};
        p.entity_type = entry.value("entity_type", 0);
        p.entity_instance_num = entry.value("entity_instance", 0);
        p.entity_container_id = entry.value("entity_container_id", 0);
        auto child_node = entry.value("associate", empty);
        std::vector<std::tuple<entity, std::string, std::string>> cvector;
        for (const auto& child_entity : child_node)
        {
            entity c{};
            std::tuple<entity, std::string, std::string> ctuple;
            c.entity_type = child_entity.value("entity_type", 0);
            c.entity_instance_num = child_entity.value("entity_instance", 0);
            c.entity_container_id =
                child_entity.value("entity_container_id", 0);
            auto association_details_arr =
                child_entity.value("association_details", emptyList);
            std::string forward = association_details_arr[0];
            std::string reverse = association_details_arr[1];
            std::get<0>(ctuple) = c;
            std::get<1>(ctuple) = forward;
            std::get<2>(ctuple) = reverse;
            cvector.push_back(ctuple);
        }
        associationsInfoMap.emplace(std::move(p), std::move(cvector));
    }
}

} // namespace host_associations
} // namespace pldm
