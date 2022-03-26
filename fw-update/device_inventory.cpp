#include "device_inventory.hpp"

#include <iostream>

namespace pldm::fw_update::device_inventory
{

Entry::Entry(sdbusplus::bus::bus& bus, const pldm::dbus::ObjectPath& objPath,
             const pldm::UUID& mctpUUID, const Associations& assocs) :
    Ifaces(bus, objPath.c_str(), true)
{
    Ifaces::type(ChassisType::Component, true);
    Ifaces::uuid(mctpUUID, true);
    Ifaces::associations(assocs, true);
    Ifaces::emit_object_added();
}

Manager::Manager(sdbusplus::bus::bus& bus,
                 const DeviceInventoryInfo& deviceInventoryInfo) :
    bus(bus),
    deviceInventoryInfo(deviceInventoryInfo)
{}

sdbusplus::message::object_path Manager::createEntry(const pldm::UUID& uuid)
{
    sdbusplus::message::object_path deviceObjPath{};
    if (deviceInventoryInfo.contains(uuid))
    {
        auto search = deviceInventoryInfo.find(uuid);
        const auto& objPath = std::get<DeviceObjPath>(search->second);
        const auto& assocs = std::get<Associations>(search->second);
        deviceEntryMap.emplace(
            uuid, std::make_unique<Entry>(bus, objPath, uuid, assocs));
        deviceObjPath = objPath;
    }
    else
    {
        std::cerr << "device_inventory::createEntry::<undetected_uuid>"
                  << "\n";
    }

    return deviceObjPath;
}

} // namespace pldm::fw_update::device_inventory