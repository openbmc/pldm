#include "fru_parser.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace pldm
{

namespace responder
{

namespace fru_parser
{

using Json = nlohmann::json;
using namespace phosphor::logging;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

const Json emptyJson{};
const std::vector<Json> emptyJsonList{};
const std::vector<std::string> emptyStringVec{};

constexpr auto fruMasterJson = "FRU_Master.json";

FruParser& get(const std::string& dir)
{
    static FruParser parser;

    if (parser.empty())
    {
        parser.setupFRUInfo(dir);
    }

    return parser;
}

void FruParser::setupFRUInfo(const std::string& dirPath)
{
    fs::path dir(dirPath);
    if (!fs::exists(dir) || fs::is_empty(dir))
    {
        std::cerr << "FRU config directory does not exist or empty, DIR="
                  << dirPath;
        throw InternalFailure();
    }

    fs::path masterFilePath = dir / fruMasterJson;
    if (!fs::exists(masterFilePath))
    {
        std::cerr << "FRU D-Bus lookup JSON does not exist, PATH="
                  << masterFilePath;
        throw InternalFailure();
    }

    setupDBusLookup(masterFilePath);
    setupFruRecordMap(dirPath);
}

void FruParser::setupDBusLookup(const fs::path& filePath)
{
    std::ifstream jsonFile(filePath);

    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        std::cerr << "Parsing FRU master config file failed, FILE=" << filePath;
        throw InternalFailure();
    }

    Service service = data.value("service", "");
    RootPath rootPath = data.value("root_path", "");
    Interfaces interfaces = data.value("interfaces", emptyStringVec);
    lookupInfo.emplace(std::make_tuple(service, rootPath, interfaces));
}

void FruParser::setupFruRecordMap(const std::string& dirPath)
{
    for (auto& file : fs::directory_iterator(dirPath))
    {
        auto fileName = file.path().filename().string();
        if (fruMasterJson == fileName)
        {
            continue;
        }

        std::ifstream jsonFile(file.path());
        auto data = Json::parse(jsonFile, nullptr, false);
        if (data.is_discarded())
        {

            std::cerr << "Parsing FRU master config file failed, FILE="
                      << file.path();
            throw InternalFailure();
        }

        auto record = data.value("record_details", emptyJson);
        auto recordType =
            static_cast<uint8_t>(record.value("fru_record_type", 0));
        auto encType = static_cast<uint8_t>(record.value("encoding_type", 0));
        auto itemIntfName = record.value("item_interface_name", "");
        auto entries = data.value("fields", emptyJsonList);
        std::vector<FieldInfo> fieldInfo;

        for (const auto& entry : entries)
        {
            auto fieldType = static_cast<uint8_t>(entry.value("field_type", 0));
            auto dbus = entry.value("dbus", emptyJson);
            auto interface = dbus.value("interface", "");
            auto property = dbus.value("property_name", "");
            auto propType = dbus.value("property_type", "");
            fieldInfo.emplace_back(
                std::make_tuple(interface, property, propType, fieldType));
        }

        FruRecordInfo fruInfo;
        fruInfo = std::make_tuple(recordType, encType, std::move(fieldInfo));

        auto search = recordMap.find(itemIntfName);
        if (search != recordMap.end())
        {
            search->second.emplace_back(std::move(fruInfo));
        }
        else
        {
            FruRecordInfos recordInfos{fruInfo};
            recordMap.emplace(itemIntfName, recordInfos);
        }
    }
}

} // namespace fru_parser

} // namespace responder

} // namespace pldm
