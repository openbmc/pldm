#pragma once

#include "dictionary.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <tuple>

namespace pldm::rde
{

constexpr std::string_view baseDictionaryRoot = "/var/lib/pldm/dict";
constexpr std::string_view dictFilePrefix = "dictionary_";
constexpr std::string_view dictFileExtension = ".bin";
constexpr std::string_view annotationFileName = "annotation.bin";

/**
 * @struct DictionaryKey
 * @brief Uniquely identifies a dictionary instance by resource ID and schema
 * class.
 */
struct DictionaryKey
{
    uint32_t resourceId;
    uint8_t schemaClass;

    bool operator<(const DictionaryKey& other) const
    {
        return std::tie(resourceId, schemaClass) <
               std::tie(other.resourceId, other.schemaClass);
    }
};

/**
 * @class DictionaryManager
 * @brief Manages multiple Dictionary instances for a single device across
 * schema/resource combinations.
 *
 * This class provides functionality to create, retrieve, and manage Dictionary
 * objects associated with specific Redfish resources and schema classes for a
 * single RDE-capable device.
 */
class DictionaryManager
{
  public:
    /**
     * @brief Constructs a DictionaryManager for a specific device.
     *
     * Initializes the dictionary manager with a device UUID and ensures that
     * the associated persistence directory exists on the filesystem.
     *
     * @param[in] uuid Unique identifier string for the device whose
     * dictionaries are managed by this instance.
     */
    explicit DictionaryManager(std::string uuid);

    DictionaryManager(const DictionaryManager&) = default;
    DictionaryManager(DictionaryManager&&) = default;
    DictionaryManager& operator=(const DictionaryManager&) = default;
    DictionaryManager& operator=(DictionaryManager&&) = default;
    ~DictionaryManager() = default;

    /**
     * @brief Get or create a dictionary instance.
     * @param[in] resourceId The resource ID associated with the dictionary.
     * @param[in] schemaClass The schema class identifier.
     * @return Reference to the Dictionary instance.
     */
    Dictionary& getOrCreate(uint32_t resourceId, uint8_t schemaClass);

    /**
     * @brief Add a chunk of dictionary data to the appropriate Dictionary
     * instance.
     * @param[in] resourceId The resource ID.
     * @param[in] schemaClass The schema class.
     * @param[in] payload The dictionary data chunk.
     * @param[in] hasChecksum Whether the payload includes a checksum byte.
     * @param[in] isFinalChunk Whether this is the final chunk of the
     * dictionary.
     * @throws std::invalid_argument if the payload is invalid.
     * @throws std::runtime_error if chunk processing or persistence fails.
     */
    void addChunk(uint32_t resourceId, uint8_t schemaClass,
                  std::span<const uint8_t> payload, bool hasChecksum,
                  bool isFinalChunk);

    /**
     * @brief Reset and remove a dictionary instance and its persistence file.
     * @param[in] resourceId The resource ID.
     * @param[in] schemaClass The schema class.
     */
    void reset(uint32_t resourceId, uint8_t schemaClass);

    /**
     * @brief Get a const pointer to a dictionary instance if it exists.
     * @param[in] resourceId The resource ID.
     * @param[in] schemaClass The schema class.
     * @return Pointer to the Dictionary instance, or nullptr if not found.
     */
    const Dictionary* get(uint32_t resourceId, uint8_t schemaClass) const;

    /**
     * @brief Get the UUID of the device associated with this manager.
     */
    const std::string& getDeviceUUID() const
    {
        return deviceUUID;
    }

    /**
     * @brief Build an annotation dictionary from a binary file.
     * @param[in] filePath Path to the annotation dictionary binary file.
     */
    void buildAnnotationDictionary(const std::string& filePath);

    /**
     * @brief Get the annotation dictionary if it exists.
     * @return Pointer to the annotation dictionary, or nullptr if not set.
     */
    const Dictionary* getAnnotationDictionary() const;

    /**
     * @brief Create a dictionary from a binary file and store it in the map.
     * @param[in] resourceId Resource ID.
     * @param[in] schemaClass Schema class.
     * @param[in] filePath Path to the binary dictionary file.
     */
    void createDictionaryFromFile(uint32_t resourceId, uint8_t schemaClass,
                                  const std::string& filePath);

    /**
     * @brief Loads all dictionary files from the root directory.
     * Iterates through the dictionary root path extracts valid filenames,
     * parses the resource ID, and delegates to createDictionaryFromFile().
     */
    void loadAllDictionariesFromRoot();

  private:
    std::string deviceUUID;
    std::filesystem::path dictRootPath;
    std::map<DictionaryKey, Dictionary> dictionaries;
    std::optional<Dictionary> annotationDictionary;
};

} // namespace pldm::rde
