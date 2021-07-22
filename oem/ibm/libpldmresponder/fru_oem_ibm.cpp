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
    auto& entityAssociationMap = getAssociateEntityMap();
    uint16_t fruRSI = record->record_set_id;

    const uint8_t* data = fruData.data();
    data += sizeof(pldm_fru_record_data_format) - sizeof(pldm_fru_record_tlv);
    for (int i = 0; i < record->num_fru_fields; i++)
    {
        auto tlv = reinterpret_cast<const pldm_fru_record_tlv*>(data);
        if ((tlv->type == PLDM_OEM_FRU_FIELD_TYPE_PCIE_CONFIG_SPACE_DATA) &&
            updateDBus)
        {
            auto pcieData =
                reinterpret_cast<const PcieConfigSpaceData*>(tlv->value);

            auto vendorId = std::to_string(htole16(pcieData->vendorId));
            auto deviceId = std::to_string(htole16(pcieData->deviceId));
            auto revisionId = std::to_string(pcieData->revisionId);

            std::stringstream ss;

            for (int ele : pcieData->classCode)
            {
                ss << std::setfill('0') << std::setw(2) << std::hex << ele;
            }
            std::string classCode = ss.str();

            auto subSystemVendorId =
                std::to_string(htole16(pcieData->subSystemVendorId));
            auto subSystemId = std::to_string(htole16(pcieData->subSystemId));

            updateDBusProperty(fruRSI, entityAssociationMap, vendorId, deviceId,
                               revisionId, classCode, subSystemVendorId,
                               subSystemId);
        }
        data += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
    }
}

void Handler::updateDBusProperty(uint16_t fruRSI,
                                 const AssociatedEntityMap& fruAssociationMap,
                                 std::string vendorId, std::string deviceId,
                                 std::string revisionId, std::string classCode,
                                 std::string subSystemVendorId,
                                 std::string subSystemId)
{
    uint16_t entityType{};
    uint16_t entityInstanceNum{};
    uint16_t containerId{};
    uint16_t terminusHandle{};
    const pldm_pdr_record* record{};

    record = pldm_pdr_fru_record_set_find_by_rsi(
        pdrRepo, fruRSI, &terminusHandle, &entityType, &entityInstanceNum,
        &containerId, true);

    if (record && (true ^ pldm_pdr_record_is_remote(record)))
    {
        for (const auto& [key, value] : fruAssociationMap)
        {
            if (entityInstanceNum == value.entity_instance_num &&
                entityType == value.entity_type &&
                containerId == value.entity_container_id)
            {
                std::string adapterObjPath = key;

                // set Vendor ID property
                pldm::utils::PropertyValue value = vendorId;
                pldm::utils::DBusMapping dbusMapping;
                dbusMapping.objectPath = adapterObjPath;
                dbusMapping.interface =
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice";
                dbusMapping.propertyName = "Function0VendorId";
                dbusMapping.propertyType = "string";
                try
                {
                    pldm::utils::DBusHandler().setDbusProperty(dbusMapping,
                                                               value);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Failed To set vendorId property"
                              << "ERROR=" << e.what() << std::endl;
                }

                // set Device Id property
                pldm::utils::PropertyValue value1 = deviceId;
                pldm::utils::DBusMapping dbusMapping1;
                dbusMapping1.objectPath = adapterObjPath;
                dbusMapping1.interface =
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice";
                dbusMapping1.propertyName = "Function0DeviceId";
                dbusMapping1.propertyType = "string";
                try
                {
                    pldm::utils::DBusHandler().setDbusProperty(dbusMapping1,
                                                               value1);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Failed To set deviceId property"
                              << "ERROR=" << e.what() << std::endl;
                }

                // set revision Id Property
                pldm::utils::PropertyValue value2 = revisionId;
                pldm::utils::DBusMapping dbusMapping2;
                dbusMapping2.objectPath = adapterObjPath;
                dbusMapping2.interface =
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice";
                dbusMapping2.propertyName = "Function0RevisionId";
                dbusMapping2.propertyType = "string";
                try
                {
                    pldm::utils::DBusHandler().setDbusProperty(dbusMapping2,
                                                               value2);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Failed To set revisionId property"
                              << "ERROR=" << e.what() << std::endl;
                }

                // set class code
                pldm::utils::PropertyValue value3 = classCode;
                pldm::utils::DBusMapping dbusMapping3;
                dbusMapping3.objectPath = adapterObjPath;
                dbusMapping3.interface =
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice";
                dbusMapping3.propertyName = "Function0ClassCode";
                dbusMapping3.propertyType = "string";
                try
                {
                    pldm::utils::DBusHandler().setDbusProperty(dbusMapping3,
                                                               value3);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Failed To set classCode property"
                              << "ERROR=" << e.what() << std::endl;
                }

                // set subsystem vendor id
                pldm::utils::PropertyValue value4 = subSystemVendorId;
                pldm::utils::DBusMapping dbusMapping4;
                dbusMapping4.objectPath = adapterObjPath;
                dbusMapping4.interface =
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice";
                dbusMapping4.propertyName = "Function0SubsystemVendorId";
                dbusMapping4.propertyType = "string";
                try
                {
                    pldm::utils::DBusHandler().setDbusProperty(dbusMapping4,
                                                               value4);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Failed To set subsystemVendorId property"
                              << "ERROR=" << e.what() << std::endl;
                }

                // set subsystem id
                pldm::utils::PropertyValue value5 = subSystemId;
                pldm::utils::DBusMapping dbusMapping5;
                dbusMapping5.objectPath = adapterObjPath;
                dbusMapping5.interface =
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice";
                dbusMapping5.propertyName = "Function0SubsystemId";
                dbusMapping5.propertyType = "string";
                try
                {
                    pldm::utils::DBusHandler().setDbusProperty(dbusMapping5,
                                                               value5);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Failed To set subsystemId property"
                              << "ERROR=" << e.what() << std::endl;
                }
                break;
            }
        }
    }
}

} // namespace oem_ibm_fru
} // namespace responder
} // namespace pldm
