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

static constexpr auto csrFilePath = "/var/lib/bmcweb/CSR";

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
    {
        vmiCertMatcher = std::make_unique<sdbusplus::bus::match::match>(
            pldm::utils::DBusHandler::getBus(),
            sdbusplus::bus::match::rules::interfacesAdded() +
                sdbusplus::bus::match::rules::argNpath(0, dumpObjPath) {
                    std::map<std::string,
                             std::map<std::string,
                                      std::variant<std::string, uint32_t>>>
                        interfaces;
                    sdbusplus::message::object_path path;
                    msg.read(path, interfaces);
                    for (auto& interface : interfaces)
                    {
                        if (interface.first == certAuthority)
                        {
                            for (const auto& property : interface.second)
                            {
                                if (property.first == "CSR")
                                {
                                    csr =
                                        std::get<std::string>(property.second);
                                }
                                else if (property.first == "ClientCertificate")
                                {
                                    clientCert =
                                        std::get<std::string>(property.second);
                                }
                            }
                            break;
                        }
                    }
                });
        csrFilePath << csr;
    }

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

    /** @brief CertHandler destructor
     */
    ~CertHandler()
    {}

  private:
    uint16_t certType;      //!< type of the certificate
    static CertMap certMap; //!< holds the fd and remaining read/write size for
                            //!< each certificate
    std::string csr;        //!< CSR String
    std::string clientCert; //!< Clent certificate string
    std::unique_ptr<sdbusplus::bus::match::match>
        vmiCertMatcher; //!< Pointer to capture the interface added signal
                        //!< for new resource dump
};
} // namespace responder
} // namespace pldm
