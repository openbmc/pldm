#include "firmware_inventory.hpp"

#include <iostream>

namespace pldm::fw_update::fw_inventory
{

Entry::Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
             const std::string& versionStr) :
    Ifaces(bus, objPath.c_str(), true)
{
    Ifaces::version(versionStr, true);
    Ifaces::purpose(VersionPurpose::Other, true);
    Ifaces::emit_object_added();
}

void Entry::createInventoryAssociation(const std::string& deviceObjPath)
{
    auto assocs = associations();
    assocs.emplace_back(
        std::make_tuple(fwdAssociation, revAssociation, deviceObjPath));
    associations(assocs);
}

Manager::Manager(sdbusplus::bus::bus& bus,
                 const FirmwareInventoryInfo& firmwareInventoryInfo,
                 const ComponentInfoMap& componentInfoMap) :
    bus(bus),
    firmwareInventoryInfo(firmwareInventoryInfo),
    componentInfoMap(componentInfoMap)
{}

void Manager::createEntry(pldm::EID eid, const pldm::UUID& uuid,
                          const sdbusplus::message::object_path& objectPath)
{
    if (firmwareInventoryInfo.contains(uuid) && componentInfoMap.contains(eid))
    {
        auto fwInfoSearch = firmwareInventoryInfo.find(uuid);
        auto compInfoSearch = componentInfoMap.find(eid);

        for (const auto& [compKey, compInfo] : compInfoSearch->second)
        {
            if (fwInfoSearch->second.contains(compKey.second))
            {
                auto componentName = fwInfoSearch->second.find(compKey.second);
                std::string objPath = swBasePath + "/" + componentName->second;
                auto entry = std::make_unique<Entry>(bus, objPath,
                                                     std::get<1>(compInfo));
                if (objectPath != "")
                {
                    entry->createInventoryAssociation(objectPath);
                }

                firmwareInventoryMap.emplace(
                    std::make_pair(eid, compKey.second), std::move(entry));
            }
        }
    }
    else
    {
        // Skip if UUID is not present or firmware inventory information from
        // firmware update config JSON is empty
    }
}

} // namespace pldm::fw_update::fw_inventory