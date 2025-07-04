#include "dictionary.hpp"

#include <filesystem>
#include <fstream>

namespace pldm::rde
{
Dictionary::Dictionary(uint32_t resourceId, uint8_t schemaClass,
                       const std::string& deviceUUID,
                       const std::string& basePath) :
    resourceId(resourceId), schemaClass(schemaClass), deviceUUID(deviceUUID),
    persistencePath(basePath)
{}

bool Dictionary::addToDictionaryBytes(std::span<const uint8_t> payload,
                                      bool hasChecksum)
{
    if (hasChecksum && !payload.empty())
    {
        // Remove the last byte (checksum)
        dictionary.insert(dictionary.end(), payload.begin(), payload.end() - 1);
    }
    else
    {
        dictionary.insert(dictionary.end(), payload.begin(), payload.end());
    }
    return true;
}

uint32_t Dictionary::getResourceId() const
{
    return resourceId;
}

uint8_t Dictionary::getSchemaClass() const
{
    return schemaClass;
}

std::span<const uint8_t> Dictionary::getDictionaryBytes() const
{
    return dictionary;
}

void Dictionary::markComplete()
{
    complete = true;
}

bool Dictionary::isComplete() const
{
    return complete;
}

void Dictionary::save() const
{
    std::ofstream outFile(persistencePath, std::ios::binary);
    if (!outFile)
        return;

    outFile.write(reinterpret_cast<const char*>(&resourceId),
                  sizeof(resourceId));
    outFile.write(reinterpret_cast<const char*>(&schemaClass),
                  sizeof(schemaClass));
    outFile.write(reinterpret_cast<const char*>(&complete), sizeof(complete));

    uint32_t size = dictionary.size();
    outFile.write(reinterpret_cast<const char*>(&size), sizeof(size));
    outFile.write(reinterpret_cast<const char*>(dictionary.data()), size);
}

void Dictionary::load()
{
    std::ifstream inFile(persistencePath, std::ios::binary);
    if (!inFile)
        return;

    inFile.read(reinterpret_cast<char*>(&resourceId), sizeof(resourceId));
    inFile.read(reinterpret_cast<char*>(&schemaClass), sizeof(schemaClass));
    inFile.read(reinterpret_cast<char*>(&complete), sizeof(complete));

    uint32_t size = 0;
    inFile.read(reinterpret_cast<char*>(&size), sizeof(size));
    dictionary.resize(size);
    inFile.read(reinterpret_cast<char*>(dictionary.data()), size);
}

void Dictionary::reset()
{
    dictionary.clear();
    complete = false;
    std::filesystem::remove(persistencePath);
}

void Dictionary::loadFromFile(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open dictionary file: " + filePath);
    }

    dictionary.assign(std::istreambuf_iterator<char>(file), {});
    complete = true;
}

} // namespace pldm::rde
