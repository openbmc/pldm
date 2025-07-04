#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pldm::rde
{
/**
 * @class Dictionary
 * @brief Represents a schema dictionary used in RDE for encoding and
 * decoding payloads.
 *
 * The Dictionary class stores the raw dictionary bytes associated with a
 * specific resource ID and schema class. It supports multipart accumulation
 * of dictionary data and provides persistence across reboots using a
 * device-specific UUID.
 */
class Dictionary
{
  public:
    /**
     * @brief Constructs a Dictionary with resource ID, schema class, and
     * device UUID.
     * @param[in] resourceId The resource ID associated with this dictionary.
     * @param[in] schemaClass The schema class identifier.
     * @param[in] deviceUUID A stable UUID used to scope persistence.
     * @param[in] path Path to the device-specific file used
     *            for persistent storage.
     */
    Dictionary(uint32_t resourceId, uint8_t schemaClass,
               const std::string& deviceUUID, const std::string& path);

    /**
     * @brief Constructor.
     */

    Dictionary(const Dictionary&) = default;
    Dictionary() = default;
    Dictionary(Dictionary&&) = default;
    Dictionary& operator=(const Dictionary&) = default;
    Dictionary& operator=(Dictionary&&) = default;

    /**
     * @brief Destructor.
     */
    ~Dictionary() = default;

    /**
     * @brief Add a chunk of dictionary bytes to the internal buffer.
     * @param[in] payload The payload chunk to append.
     * @param[in] hasChecksum Indicates whether the payload includes a checksum.
     * @return True if the chunk was added successfully.
     */
    bool addToDictionaryBytes(std::span<const uint8_t> payload,
                              bool hasChecksum);

    /**
     * @brief Get the resource ID associated with this dictionary.
     * @return The resource ID.
     */
    uint32_t getResourceId() const;

    /**
     * @brief Get the schema class associated with this dictionary.
     * @return The schema class.
     */
    uint8_t getSchemaClass() const;

    /**
     * @brief Get a span over the accumulated dictionary bytes.
     * @return A span of the dictionary byte buffer.
     */
    std::span<const uint8_t> getDictionaryBytes() const;

    /**
     * @brief Mark the dictionary as complete.
     */
    void markComplete();

    /**
     * @brief Check if the dictionary is complete.
     * @return True if complete, false otherwise.
     */
    bool isComplete() const;

    /**
     * @brief Save the dictionary state to a persistent file.
     */
    void save() const;

    /**
     * @brief Load the dictionary state from a persistent file.
     */
    void load();

    /**
     * @brief Reset the dictionary state and remove the persistence file.
     */
    void reset();

    /**
     * @brief Retrieves the dictionary data.
     *
     * This function returns the complete dictionary as a vector of bytes.
     * It can be used for serialization, inspection, or communication purposes.
     *
     * @return A vector containing the dictionary bytes.
     */
    std::vector<uint8_t> getDictionary() const;

    /**
     * @brief Load dictionary bytes from a binary file.
     * @param filePath Path to the binary file.
     * @throws std::runtime_error if file can't be read.
     */
    void loadFromFile(const std::string& filePath);

  private:
    /**
     * @brief full path to the persistence file.
     */
    uint32_t resourceId = 0;
    uint8_t schemaClass = 0;
    std::string deviceUUID;
    std::string persistencePath;
    std::vector<uint8_t> dictionary;
    bool complete = false;
};

} // namespace pldm::rde
