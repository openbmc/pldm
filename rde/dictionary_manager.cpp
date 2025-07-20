#include "dictionary_manager.hpp"

#include <phosphor-logging/lg2.hpp>

extern "C"
{
#include "libpldm/rde.h"
}
#include <stdexcept>

PHOSPHOR_LOG2_USING;

namespace pldm::rde
{

DictionaryManager::DictionaryManager(std::string deviceUUID) :
    deviceUUID(std::move(deviceUUID)),
    dictRootPath(std::string(baseDictionaryRoot) + "/" + deviceUUID)
{
    std::filesystem::create_directories(dictRootPath);
    buildAnnotationDictionary(dictRootPath / std::string(annotationFileName));

    const std::string triggerFile = "/tmp/.enable_dict_bootstrap";
    if (std::filesystem::exists(triggerFile))
    {
        info("RDE: loading dictionaries from persistent store");
        loadAllDictionariesFromRoot(); // Load from persistent location
    }
}

Dictionary& DictionaryManager::getOrCreate(uint32_t resourceId,
                                           uint8_t schemaClass)
{
    DictionaryKey key{resourceId, schemaClass};
    std::filesystem::path dictFilePath =
        dictRootPath /
        (std::string(dictFilePrefix) + std::to_string(resourceId) +
         std::string(dictFileExtension));

    auto [it, inserted] = dictionaries.emplace(
        key, Dictionary(resourceId, schemaClass, deviceUUID, dictFilePath));
    return it->second;
}

void DictionaryManager::addChunk(uint32_t resourceId, uint8_t schemaClass,
                                 std::span<const uint8_t> payload,
                                 bool hasChecksum, bool isFinalChunk)
{
    info("RDE: DictionaryManager addChunk Enter");

    if (payload.empty())
    {
        throw std::invalid_argument("Payload chunk is empty.");
    }

    Dictionary& dict = getOrCreate(resourceId, schemaClass);

    if (!dict.addToDictionaryBytes(payload, hasChecksum))
    {
        throw std::runtime_error("Failed to add chunk to dictionary.");
    }

    if (isFinalChunk)
    {
        dict.markComplete();
        try
        {
            dict.save();
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(
                std::string("Failed to persist dictionary: ") + e.what());
        }
    }
    info("RDE: DictionaryManager addChunk Exit");
}

void DictionaryManager::reset(uint32_t resourceId, uint8_t schemaClass)
{
    DictionaryKey key{resourceId, schemaClass};
    auto it = dictionaries.find(key);
    if (it != dictionaries.end())
    {
        it->second.reset();
        dictionaries.erase(it);
    }
}

const Dictionary* DictionaryManager::get(uint32_t resourceId,
                                         uint8_t schemaClass) const
{
    DictionaryKey key{resourceId, schemaClass};
    auto it = dictionaries.find(key);
    return (it != dictionaries.end()) ? &it->second : nullptr;
}

void DictionaryManager::buildAnnotationDictionary(const std::string& filePath)
{
    Dictionary dict(0, PLDM_RDE_SCHEMA_ANNOTATION, deviceUUID, filePath);
    dict.loadFromFile(filePath);
    dict.save();
    annotationDictionary = std::move(dict);
}

const Dictionary* DictionaryManager::getAnnotationDictionary() const
{
    return annotationDictionary ? &(*annotationDictionary) : nullptr;
}

void DictionaryManager::loadAllDictionariesFromRoot()
{
    for (const auto& entry : std::filesystem::directory_iterator(dictRootPath))
    {
        if (!entry.is_regular_file())
            continue;

        const std::string filenameStr = entry.path().filename().string();
        std::string_view filename = filenameStr;

        if (filename.starts_with(dictFilePrefix) &&
            filename.ends_with(dictFileExtension))
        {
            size_t start = dictFilePrefix.length();
            size_t end = filename.length() - dictFileExtension.length();

            // Extract core ID substring
            std::string resourceIdStr =
                std::string{filename.substr(start, end - start)};

            if (resourceIdStr.empty())
                continue;

            try
            {
                uint32_t resourceId = std::stoul(resourceIdStr);
                createDictionaryFromFile(resourceId, /*schemaClass=*/0,
                                         entry.path().string());
            }
            catch (const std::exception& ex)
            {
                info(
                    "DictionaryManager: Skipping file '{FILE}' — invalid resourceId: {ERROR}",
                    "FILE", entry.path().filename().string(), "ERROR",
                    ex.what());
            }
        }
    }
}

void DictionaryManager::createDictionaryFromFile(
    uint32_t resourceId, uint8_t schemaClass, const std::string& filePath)
{
    info("RDE: Loading Dictionary file '{NAME}'", "NAME", filePath);
    Dictionary& dict = getOrCreate(resourceId, schemaClass);
    dict.loadFromFile(filePath);
    dict.save();
}
} // namespace pldm::rde
