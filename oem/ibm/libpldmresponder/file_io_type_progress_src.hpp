#pragma once

#include "file_io_by_type.hpp"

namespace pldm
{

namespace responder
{

/** @class ProgressCodeHandler
 *
 * @brief Inherits and implemented FileHandler. This class is used
 * to read the Progress SRC's from the Host.
 */
class ProgressCodeHandler : public FileHandler
{
  public:
    /** @brief ProgressCodeHandler constructor
     */
    ProgressCodeHandler(uint32_t fileHandle) : FileHandler(fileHandle) {}

    int writeFromMemory(uint32_t /*offset*/, uint32_t /*length*/,
                        uint64_t /*address*/,
                        oem_platform::Handler* /*oemPlatformHandler*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    int write(const char* buffer, uint32_t offset, uint32_t& length,
              oem_platform::Handler* oemPlatformHandler) override;

    int readIntoMemory(uint32_t /*offset*/, uint32_t /*length*/,
                       uint64_t /*address*/,
                       oem_platform::Handler* /*oemPlatformHandler*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    int read(uint32_t /*offset*/, uint32_t& /*length*/, Response& /*response*/,
             oem_platform::Handler* /*oemPlatformHandler*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    int fileAck(uint8_t /*fileStatus*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    int newFileAvailable(uint64_t /*length*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    virtual int newFileAvailableWithMetaData(
        uint64_t /*length*/, uint32_t /*metaDataValue1*/,
        uint32_t /*metaDataValue2*/, uint32_t /*metaDataValue3*/,
        uint32_t /*metaDataValue4*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    /** @brief method to set the dbus Raw value Property with
     * the obtained progress code from the host.
     *
     *  @param[in] progressCodeBuffer - the progress Code SRC Buffer
     */
    virtual int setRawBootProperty(
        const std::tuple<std::vector<uint8_t>, std::vector<uint8_t>>&
            progressCodeBuffer);

    virtual int fileAckWithMetaData(
        uint8_t /*fileStatus*/, uint32_t /*metaDataValue1*/,
        uint32_t /*metaDataValue2*/, uint32_t /*metaDataValue3*/,
        uint32_t /*metaDataValue4*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    /** @brief ProgressCodeHandler destructor
     */

    ~ProgressCodeHandler() {}
};

} // namespace responder
} // namespace pldm
