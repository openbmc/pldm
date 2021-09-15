#pragma once

#include "file_io_by_type.hpp"

#include <tuple>

namespace pldm
{
namespace responder
{

using LicType = uint16_t;

/** @class LicenseHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to read license
 */
class LicenseHandler : public FileHandler
{
  public:
    /** @brief Handler constructor
     */
    LicenseHandler(uint32_t fileHandle, uint16_t fileType) :
        FileHandler(fileHandle), licType(fileType)
    {}

    virtual int read(uint32_t offset, uint32_t& length, Response& response,
                     oem_platform::Handler* /*oemPlatformHandler*/);

    virtual int fileAckWithMetaData(uint32_t metaDataValue);

    /** @brief CertHandler destructor
     */
    ~LicenseHandler()
    {}

  private:
    uint16_t licType; //!< type of the certificate
};
} // namespace responder
} // namespace pldm
~
