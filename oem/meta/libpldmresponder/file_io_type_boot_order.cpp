#include "file_io_type_boot_order.hpp"

#include "oem/meta/utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Control/Boot/Mode/server.hpp>
#include <xyz/openbmc_project/Control/Boot/Source/server.hpp>
#include <xyz/openbmc_project/Control/Boot/Type/server.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::responder::oem_meta
{

constexpr size_t BOOT_ORDER_LENGTH = 6;

constexpr auto bootFlagsIntf = "xyz.openbmc_project.Control.Boot.Flags";
constexpr auto bootEnableIntf = "xyz.openbmc_project.Object.Enable";
constexpr auto bootModeIntf = "xyz.openbmc_project.Control.Boot.Mode";
constexpr auto bootSourceIntf = "xyz.openbmc_project.Control.Boot.Source";
constexpr auto bootTypeIntf = "xyz.openbmc_project.Control.Boot.Type";

constexpr auto bootCmosProp = "CMOSClear";
constexpr auto bootEnableProp = "Enabled";
constexpr auto bootModeProp = "BootMode";
constexpr auto bootOrderProp = "BootOrder";
constexpr auto bootSourceProp = "BootSource";
constexpr auto bootTypeProp = "BootType";

constexpr auto slotInterface = "xyz.openbmc_project.Inventory.Decorator.Slot";
constexpr auto slotNumberProperty = "SlotNumber";
const std::vector<std::string> slotInterfaces = {slotInterface};

using BootSource =
    sdbusplus::xyz::openbmc_project::Control::Boot::server::Source::Sources;
using BootMode =
    sdbusplus::xyz::openbmc_project::Control::Boot::server::Mode::Modes;
using BootType =
    sdbusplus::xyz::openbmc_project::Control::Boot::server::Type::Types;

using IpmiValue = uint8_t;

std::map<IpmiValue, BootSource> sourceIpmiToDbus = {
    {0x0f, BootSource::Default},       {0x00, BootSource::USB},
    {0x01, BootSource::IPv4},          {0x02, BootSource::Disk},
    {0x03, BootSource::ExternalMedia}, {0x04, BootSource::RemovableMedia},
    {0x09, BootSource::IPv6}};

std::map<IpmiValue, BootMode> modeIpmiToDbus = {{0x04, BootMode::Setup},
                                                {0x00, BootMode::Regular}};

std::map<IpmiValue, BootType> typeIpmiToDbus = {{0x00, BootType::Legacy},
                                                {0x01, BootType::EFI}};

std::map<std::optional<BootSource>, IpmiValue> sourceDbusToIpmi = {
    {BootSource::Default, 0x0f},
    {BootSource::RemovableMedia, 0x04},
    {BootSource::IPv4, 0x01},
    {BootSource::Disk, 0x02},
    {BootSource::ExternalMedia, 0x03},
    {BootSource::IPv6, 0x09},
    {BootSource::USB, 0x00}};

std::map<std::optional<BootMode>, IpmiValue> modeDbusToIpmi = {
    {BootMode::Setup, 0x04}, {BootMode::Regular, 0x00}};

std::map<std::optional<BootType>, IpmiValue> typeDbusToIpmi = {
    {BootType::Legacy, 0x00}, {BootType::EFI, 0x01}};

/* Default boot order 0100090203ff */
/*
Byte 2-6– Boot sequence
    Bit 2:0 – boot device id
        000b: USB device
        001b: Network
        010b: SATA HDD
        011b: SATA-CDROM
        100b: Other removable Device
    Bit 7:3 – reserve for boot device special request
        If Bit 2:0 is 001b (Network), Bit3 is IPv4/IPv6 order
            Bit3=0b: IPv4 first
            Bit3=1b: IPv6 first
*/

int BootOrderHandler::read(struct pldm_oem_meta_file_io_read_resp* data)
{
    if (data == nullptr)
    {
        error("Input data pointer is NULL");
        return PLDM_ERROR;
    }

    try
    {
        pldm::utils::DBusMapping dbusMapping;
        std::string bootObjPath =
            "/xyz/openbmc_project/control/host" +
            pldm::oem_meta::getSlotNumberStringByTID(tid) + "/boot";

        // Get the boot mode
        auto value = dBusIntf->getDbusPropertyVariant(
            bootObjPath.c_str(), bootModeProp, bootModeIntf);
        auto bootMode = sdbusplus::message::convert_from_string<BootMode>(
            std::get<std::string>(value));
        uint8_t bootOption = modeDbusToIpmi.at(bootMode);

        // Get the boot type
        value = dBusIntf->getDbusPropertyVariant(bootObjPath.c_str(),
                                                 bootTypeProp, bootTypeIntf);
        auto bootType = sdbusplus::message::convert_from_string<BootType>(
            std::get<std::string>(value));
        uint8_t bootTypeVal = typeDbusToIpmi.at(bootType);

        // Get the boot sequence (Byte 2-6)
        value = dBusIntf->getDbusPropertyVariant(bootObjPath.c_str(),
                                                 bootOrderProp, bootSourceIntf);
        auto bootOrderStr = std::get<std::vector<std::string>>(value);
        std::vector<BootSource> bootOrderVal;
        for (const auto& s : bootOrderStr)
        {
            bootOrderVal.push_back(
                sdbusplus::message::convert_from_string<BootSource>(s).value());
        }

        // Get the CMOS clear bit
        value = dBusIntf->getDbusPropertyVariant(bootObjPath.c_str(),
                                                 bootCmosProp, bootFlagsIntf);
        bool cmosClearFlag = std::get<bool>(value);

        // Get the valid bit
        value = dBusIntf->getDbusPropertyVariant(
            bootObjPath.c_str(), bootEnableProp, bootEnableIntf);
        bool validFlag = std::get<bool>(value);

        std::vector<uint8_t> bootOrder;
        bootOrder.push_back(bootOption | bootTypeVal);

        if (validFlag)
        {
            bootOrder.front() |= BOOT_MODE_BOOT_FLAG;
        }

        if (cmosClearFlag)
        {
            bootOrder.front() |= BOOT_MODE_CMOS_CLR;
        }

        for (auto& bootSource : bootOrderVal)
        {
            bootOrder.push_back(sourceDbusToIpmi.at(bootSource));
        }

        // If the length of bootOrder is less than BOOT_ORDER_LENGTH, pad it
        // with 0xff.
        while (bootOrder.size() < BOOT_ORDER_LENGTH)
        {
            bootOrder.push_back(0xff);
        }

        memcpy(pldm_oem_meta_file_io_read_resp_data(data), bootOrder.data(),
               bootOrder.size());

        data->length = BOOT_ORDER_LENGTH;
        data->info.data.transferFlag = PLDM_END;
        data->info.data.offset = 0;
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("{FUNC}: Failed to execute Dbus call, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return PLDM_ERROR;
    }
    catch (const std::exception& e)
    {
        error("{FUNC}: Unexpected exception occurred, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

int BootOrderHandler::write(const message& data)
{
    if (data.size() != BOOT_ORDER_LENGTH)
    {
        error(
            "{FUNC}: Invalid incoming data for setting boot order, data size={SIZE}",
            "FUNC", std::string(__func__), "SIZE", data.size());
        return PLDM_ERROR;
    }

    try
    {
        pldm::utils::DBusMapping dbusMapping;
        dbusMapping.objectPath =
            "/xyz/openbmc_project/control/host" +
            pldm::oem_meta::getSlotNumberStringByTID(tid) + "/boot";

        uint8_t mode = data[0];

        // Set property of CMOS clear
        bool cmosClear = (mode & BOOT_MODE_CMOS_CLR) ? true : false;
        dbusMapping.interface = std::string(bootFlagsIntf);
        dbusMapping.propertyName = std::string(bootCmosProp);
        dbusMapping.propertyType = "bool";
        dBusIntf->setDbusProperty(dbusMapping, cmosClear);

        // Set property of vaild
        bool bootOrderEnable = (mode & BOOT_MODE_BOOT_FLAG) ? true : false;
        dbusMapping.interface = std::string(bootEnableIntf);
        dbusMapping.propertyName = std::string(bootEnableProp);
        dbusMapping.propertyType = "bool";
        dBusIntf->setDbusProperty(dbusMapping, bootOrderEnable);

        // Set property of boot mode
        uint8_t bootModeBit = mode & 0x04;
        auto bootModeVal = modeIpmiToDbus.at(bootModeBit);
        std::string bootMode =
            sdbusplus::message::convert_to_string<BootMode>(bootModeVal);
        dbusMapping.interface = std::string(bootModeIntf);
        dbusMapping.propertyName = std::string(bootModeProp);
        dbusMapping.propertyType = "string";
        dBusIntf->setDbusProperty(dbusMapping, bootMode);

        // Set property of boot type
        uint8_t bootTypeBit = mode & 0x01;
        auto bootTypeVal = typeIpmiToDbus.at(bootTypeBit);
        std::string bootType =
            sdbusplus::message::convert_to_string<BootType>(bootTypeVal);
        dbusMapping.interface = std::string(bootTypeIntf);
        dbusMapping.propertyName = std::string(bootTypeProp);
        dbusMapping.propertyType = "string";
        dBusIntf->setDbusProperty(dbusMapping, bootType);

        // Set property of boot sequence
        std::vector<std::string> bootOrderStr;
        if (data[1] == 0xff)
        {
            error("{FUNC}: First boot order value should not be reserved",
                  "FUNC", std::string(__func__));
            return PLDM_ERROR;
        }

        for (size_t i = 1; i < BOOT_ORDER_LENGTH; ++i)
        {
            if (data[i] == 0xff)
            {
                break;
            }
            auto it = sourceIpmiToDbus.find(data[i]);
            if (it != sourceIpmiToDbus.end())
            {
                bootOrderStr.push_back(
                    sdbusplus::message::convert_to_string<BootSource>(
                        it->second));
            }
            else
            {
                error("{FUNC}: Invalid boot sequence value: {VAL}", "FUNC",
                      std::string(__func__), "VAL", data[i]);
                return PLDM_ERROR;
            }
        }
        dbusMapping.interface = std::string(bootSourceIntf);
        dbusMapping.propertyName = std::string(bootOrderProp);
        dbusMapping.propertyType = "array[string]";
        dBusIntf->setDbusProperty(dbusMapping, bootOrderStr);

        // Set property of boot source
        auto bootSourceVal = sourceIpmiToDbus.at(data[1]);
        std::string bootSource =
            sdbusplus::message::convert_to_string<BootSource>(bootSourceVal);
        dbusMapping.interface = std::string(bootSourceIntf);
        dbusMapping.propertyName = std::string(bootSourceProp);
        dbusMapping.propertyType = "string";
        dBusIntf->setDbusProperty(dbusMapping, bootSource);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("{FUNC}: Failed to execute Dbus call, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return PLDM_ERROR;
    }
    catch (const std::exception& e)
    {
        error("{FUNC}: Unexpected exception occurred, ERROR={ERROR}", "FUNC",
              std::string(__func__), "ERROR", e.what());
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
