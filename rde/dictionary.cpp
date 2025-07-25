#include "dictionary.hpp"

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <fstream>

PHOSPHOR_LOG2_USING;

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
    size_t actualPayloadSize = payload.size();
    info("RDE: addToDictionaryBytespayloadSize={SIZE}, hasChecksum={CHECKSUM}",
         "SIZE", actualPayloadSize, "CHECKSUM", hasChecksum);

    if (hasChecksum && actualPayloadSize >= sizeof(uint32_t))
    {
        actualPayloadSize -= sizeof(uint32_t); // trim off checksum
    }
    dictionary.insert(dictionary.end(), payload.begin(),
                      payload.begin() + actualPayloadSize);

    info("RDE: addToDictionaryBytespayloadSize={SIZE}, hasChecksum={CHECKSUM}",
         "SIZE", actualPayloadSize, "CHECKSUM", hasChecksum);

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
    {
        error("RDE: Failed to open file for saving dictionary at path={PATH}",
              "PATH", persistencePath);
        return;
    }

    outFile.write(reinterpret_cast<const char*>(dictionary.data()),
                  dictionary.size());
    outFile.flush(); // Ensure all data is written
}

void Dictionary::load()
{
    std::ifstream inFile(persistencePath, std::ios::binary | std::ios::ate);
    if (!inFile)
    {
        error("RDE: Failed to open file for loading dictionary at path={PATH}",
              "PATH", persistencePath);
        return;
    }

    std::streamsize size = inFile.tellg();
    if (size <= 0)
    {
        error("RDE: Invalid dictionary size while loading (size={SIZE})",
              "SIZE", static_cast<int>(size));
        return;
    }

    inFile.seekg(0);
    std::vector<uint8_t> temp(static_cast<size_t>(size));
    inFile.read(reinterpret_cast<char*>(temp.data()), size);
    std::streamsize bytesRead = inFile.gcount();

    if (bytesRead == size)
    {
        dictionary = std::move(temp);
    }
    else
    {
        error("RDE: Mismatch in read size. Expected={EXPECTED} Actual={ACTUAL}",
              "EXPECTED", static_cast<int>(size), "ACTUAL",
              static_cast<int>(bytesRead));
        dictionary.clear(); // Optional safeguard
    }
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
