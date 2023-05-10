#pragma once

#include "file_io_by_type.hpp"

#include <tuple>

namespace pldm
{
namespace responder
{
using Fd = int;
using RemainingSize = uint64_t;
using CertDetails = std::tuple<Fd, RemainingSize>;
using CertType = uint16_t;
using CertMap = std::map<CertType, CertDetails>;

/** @class CertHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to read/write certificates and certificate signing requests
 */
class CertHandler : public FileHandler
{
  public:
    /** @brief CertHandler constructor
     */
    CertHandler(uint32_t fileHandle, uint16_t fileType) :
        FileHandler(fileHandle), certType(fileType)
    {}

    virtual int writeFromMemory(uint32_t offset, uint32_t length,
                                uint64_t address,
                                oem_platform::Handler* /*oemPlatformHandler*/);
    virtual int readIntoMemory(uint32_t offset, uint32_t& length,
                               uint64_t address,
                               oem_platform::Handler* /*oemPlatformHandler*/);
    virtual int read(uint32_t offset, uint32_t& length, Response& response,
                     oem_platform::Handler* /*oemPlatformHandler*/);

    virtual int write(const char* buffer, uint32_t offset, uint32_t& length,
                      oem_platform::Handler* /*oemPlatformHandler*/);

    virtual int fileAck(uint8_t /*fileStatus*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    virtual int newFileAvailable(uint64_t length);

    virtual int fileAckWithMetaData(uint8_t /*fileStatus*/,
                                    uint32_t /*metaDataValue1*/,
                                    uint32_t /*metaDataValue2*/,
                                    uint32_t /*metaDataValue3*/,
                                    uint32_t /*metaDataValue4*/);

    virtual int newFileAvailableWithMetaData(uint64_t length,
                                             uint32_t metaDataValue1,
                                             uint32_t /*metaDataValue2*/,
                                             uint32_t /*metaDataValue3*/,
                                             uint32_t /*metaDataValue4*/);

    /** @brief CertHandler destructor
     */
    ~CertHandler() {}

  private:
    uint16_t certType;      //!< type of the certificate
    static CertMap certMap; //!< holds the fd and remaining read/write size for
                            //!< each certificate
    enum SignedCertStatus
    {
        PLDM_INVALID_CERT_DATA = 0X03
    };
};
} // namespace responder
} // namespace pldm
