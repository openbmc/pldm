#include "fru_oem_ibm.hpp"

namespace pldm
{
namespace responder
{
namespace oem_ibm_fru
{

void pldm::responder::oem_ibm_fru::Handler::setFruHandler(
    pldm::responder::fru::Handler* handler)
{
    fruHandler = handler;
}

void pldm::responder::oem_ibm_fru::Handler::processOEMfruRecord(
    const std::vector<uint8_t>& fruData, bool updateDBus)
{
    auto record =
        reinterpret_cast<const pldm_fru_record_data_format*>(fruData.data());

    for (int i = 0; i < record->num_fru_fields; i++)
    {
        auto tlv = reinterpret_cast<const pldm_fru_record_tlv*>(fruData.data());

        if ((tlv->type == 253) && updateDBus)
        {
            std::cerr << "inside the data" << std::endl;
            /*  auto pciValuePtr = tlv->value;

              auto vendorId = (unsigned int)pciValuePtr >> 2 & 0xffff;
              auto deviceId = ((unsigned int)pciValuePtr + 2 ) >> 2 & 0xffff;
              auto revisionId =((unsigned int)pciValuePtr + 8) >> 1 & 0xff;
              auto classCode = ((unsigned int)pciValuePtr + 9) >> 3 & 0xffffff;
              auto subSystemVendorId =((unsigned int) pciValuePtr + 44) >> 2 &
              0xffff;
              auto subSystemId =((unsigned int) pciValuePtr + 44) >> 2 & 0xffff;
            */
        }
    }
}

} // namespace oem_ibm_fru
} // namespace responder
} // namespace pldm
