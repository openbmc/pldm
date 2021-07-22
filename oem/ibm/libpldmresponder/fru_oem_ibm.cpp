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
    std::cerr << " Set fru handler in fru_oem_ibm.cpp" << std::endl;
    fruHandler = handler;
}

void pldm::responder::oem_ibm_fru::Handler::processOEMfruRecord(
    const std::vector<uint8_t>& fruData, bool updateDBus)
{
    std::cerr << " Inside the processOEMfru record function" << std::endl;
    auto record =
        reinterpret_cast<const pldm_fru_record_data_format*>(fruData.data());
    auto& entityAssociationMap = getAssociateEntityMap();
    uint16_t fruRSI = record->record_set_id;
    std::cerr << "RSI : " << fruRSI << std::endl;

    auto tlv = reinterpret_cast<const pldm_fru_record_tlv*>(fruData.data());

    if ((tlv->type == 253) && updateDBus)
    {
        std::cerr << "inside the data" << std::endl;
        // const pldm_entity entity = getEntityIDfromRSI(fruRSI);

        auto pciValuePtr = tlv->value;

        auto vendorId = std::to_string((unsigned int)pciValuePtr >> 2 & 0xffff);
        auto deviceId =
            std::to_string(((unsigned int)pciValuePtr + 2) >> 2 & 0xffff);
        auto revisionId =
            std::to_string(((unsigned int)pciValuePtr + 8) >> 1 & 0xff);
        auto classCode =
            std::to_string(((unsigned int)pciValuePtr + 9) >> 3 & 0xffffff);
        auto subSystemVendorId =
            std::to_string(((unsigned int)pciValuePtr + 44) >> 2 & 0xffff);
        auto subSystemId =
            std::to_string(((unsigned int)pciValuePtr + 44) >> 2 & 0xffff);
        std::cerr << " vendorID: deviceId: revisionId: classCode : "
                     "subSystemVendorId: subSystemId: "
                  << vendorId << deviceId << revisionId << classCode
                  << subSystemVendorId << subSystemId << std::endl;

        std::cerr << "before calling update dbus" << std::endl;
        updateDBusProperty(fruRSI, entityAssociationMap, vendorId, deviceId,
                           revisionId, classCode, subSystemVendorId,
                           subSystemId);
    }
}
pldm_entity Handler::getEntityIDfromRSI(uint16_t fruRSI)
{
    std::cerr << " Inside getEntityID from RSI function" << std::endl;
    pldm_entity fruentity{};
    uint8_t* pdrData = nullptr;
    uint32_t pdrSize{};
    const pldm_pdr_record* record{};
    do
    {
        record = pldm_pdr_find_record_by_type(pdrRepo, PLDM_PDR_FRU_RECORD_SET,
                                              record, &pdrData, &pdrSize);
        if (record && (true ^ pldm_pdr_record_is_remote(record)))
        {
            auto pdr = reinterpret_cast<pldm_pdr_fru_record_set*>(pdrData);
            if (fruRSI == pdr->fru_rsi)
            {
                fruentity.entity_type = pdr->entity_type;
                fruentity.entity_instance_num = pdr->entity_instance_num;
                fruentity.entity_container_id = pdr->container_id;

                return fruentity;
            }
        }
    } while (record);

    return fruentity;
}

void Handler::updateDBusProperty(uint16_t fruRSI,
                                 const AssociatedEntityMap& fruAssociationMap,
                                 std::string vendorId, std::string deviceId,
                                 std::string revisionId, std::string classCode,
                                 std::string subSystemVendorId,
                                 std::string subSystemId)
{
    std::cerr << " Inside updateDbus property function" << std::endl;
    const pldm_entity entity = getEntityIDfromRSI(fruRSI);

    for (const auto& [key, value] : fruAssociationMap)
    {
        std::cerr << (unsigned)value.entity_type << std::endl;
        std::cerr << (unsigned)value.entity_instance_num << std::endl;
        std::cerr << (unsigned)value.entity_container_id << std::endl;
        if (entity.entity_instance_num == value.entity_instance_num &&
            entity.entity_type == value.entity_type &&
            entity.entity_container_id == value.entity_container_id)
        {
            std::string adapterObjPath = key;

            std::cerr << " Adapter Object path:" << adapterObjPath << std::endl;
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
                pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
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
        }
    }
}

} // namespace oem_ibm_fru
} // namespace responder
} // namespace pldm
