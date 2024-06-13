#include "oem_meta_file_io_type_http_boot.hpp"

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm::responder::oem_meta
{
int HttpBootHandler::read(std::vector<uint8_t>& data)
{
    static constexpr auto dbusObjPath = "/xyz/openbmc_project/Cert/http_boot";
    static constexpr auto namesProperty = "CertificateString";
    static constexpr auto dbusInterface =
        "xyz.openbmc_project.Certs.Certificate";

    std::string HttpBootString;

    uint8_t length = data.at(0);
    uint8_t trasferFlag = data.at(1);
    uint8_t highOffset = data.at(2);
    uint8_t lowOffset = data.at(3);
    uint16_t offset = (static_cast<uint16_t>(highOffset) << 8) |
                      static_cast<uint16_t>(lowOffset);
    try
    {
        HttpBootString =
            pldm::utils::DBusHandler().getDbusProperty<std::string>(
                dbusObjPath, namesProperty, dbusInterface);

        if (offset + length >= HttpBootString.size())
        {
            trasferFlag = PLDM_END;
        }
        else
        {
            trasferFlag = PLDM_MIDDLE;
        }

        if (trasferFlag == PLDM_MIDDLE)
        {
            data.insert(data.end(), HttpBootString.begin() + offset,
                        HttpBootString.begin() + offset + length);
        }
        else if (trasferFlag == PLDM_END)
        {
            data.insert(data.end(), HttpBootString.begin() + offset,
                        HttpBootString.end());

            length = HttpBootString.size() - offset; // Revise length
        }
        offset = offset + length;
        highOffset = (offset >> 8) & 0xFF;
        lowOffset = offset & 0xFF;
        data.at(0) = length;
        data.at(1) = trasferFlag;
        data.at(2) = highOffset;
        data.at(3) = lowOffset;
    }
    catch (const std::exception& e)
    {
        error("Failed to get Http boot certification. ERROR={ERROR}", "ERROR",
              e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
