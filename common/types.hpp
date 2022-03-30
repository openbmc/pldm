#pragma once

#include <stdint.h>

#include <sdbusplus/message/types.hpp>

#include <bitset>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace pldm
{

using EID = uint8_t;
using UUID = std::string;
using Request = std::vector<uint8_t>;
using Response = std::vector<uint8_t>;
using Command = uint8_t;

namespace dbus
{

using ObjectPath = std::string;
using Service = std::string;
using Interface = std::string;
using Interfaces = std::vector<std::string>;
using Property = std::string;
using PropertyType = std::string;
using Value =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string, std::vector<uint8_t>>;

using PropertyMap = std::map<Property, Value>;
using InterfaceMap = std::map<Interface, PropertyMap>;
using ObjectValueTree = std::map<sdbusplus::message::object_path, InterfaceMap>;

} // namespace dbus

namespace fw_update
{

// Descriptor definition
using DescriptorType = uint16_t;
using DescriptorData = std::vector<uint8_t>;
using VendorDefinedDescriptorTitle = std::string;
using VendorDefinedDescriptorData = std::vector<uint8_t>;
using VendorDefinedDescriptorInfo =
    std::tuple<VendorDefinedDescriptorTitle, VendorDefinedDescriptorData>;
using Descriptors =
    std::map<DescriptorType,
             std::variant<DescriptorData, VendorDefinedDescriptorInfo>>;

using DescriptorMap = std::unordered_map<EID, Descriptors>;

// Component information
using CompClassification = uint16_t;
using CompIdentifier = uint16_t;
using CompKey = std::pair<CompClassification, CompIdentifier>;
using CompClassificationIndex = uint8_t;
using CompVersion = std::string;
using CompInfo = std::tuple<CompClassificationIndex, CompVersion>;
using ComponentInfo = std::map<CompKey, CompInfo>;
using ComponentInfoMap = std::unordered_map<EID, ComponentInfo>;

// PackageHeaderInformation
using PackageHeaderSize = size_t;
using PackageVersion = std::string;
using ComponentBitmapBitLength = uint16_t;
using PackageHeaderChecksum = uint32_t;

// FirmwareDeviceIDRecords
using DeviceIDRecordCount = uint8_t;
using DeviceUpdateOptionFlags = std::bitset<32>;
using ApplicableComponents = std::vector<size_t>;
using ComponentImageSetVersion = std::string;
using FirmwareDevicePackageData = std::vector<uint8_t>;
using FirmwareDeviceIDRecord =
    std::tuple<DeviceUpdateOptionFlags, ApplicableComponents,
               ComponentImageSetVersion, Descriptors,
               FirmwareDevicePackageData>;
using FirmwareDeviceIDRecords = std::vector<FirmwareDeviceIDRecord>;

// ComponentImageInformation
using ComponentImageCount = uint16_t;
using CompComparisonStamp = uint32_t;
using CompOptions = std::bitset<16>;
using ReqCompActivationMethod = std::bitset<16>;
using CompLocationOffset = uint32_t;
using CompSize = uint32_t;
using ComponentImageInfo =
    std::tuple<CompClassification, CompIdentifier, CompComparisonStamp,
               CompOptions, ReqCompActivationMethod, CompLocationOffset,
               CompSize, CompVersion>;
using ComponentImageInfos = std::vector<ComponentImageInfo>;

// DeviceInventory
using DeviceObjPath = std::string;
using Associations =
    std::vector<std::tuple<std::string, std::string, std::string>>;
using DeviceInfo = std::tuple<DeviceObjPath, Associations>;
using DeviceInventoryInfo = std::unordered_map<UUID, DeviceInfo>;

// FirmwareInventory
using ComponentName = std::string;
using ComponentIdNameMap = std::unordered_map<CompIdentifier, ComponentName>;
using FirmwareInventoryInfo = std::unordered_map<UUID, ComponentIdNameMap>;

// ComponentInformation
using ComponentNameMapInfo = std::unordered_map<UUID, ComponentIdNameMap>;

enum class ComponentImageInfoPos : size_t
{
    CompClassificationPos = 0,
    CompIdentifierPos = 1,
    CompComparisonStampPos = 2,
    CompOptionsPos = 3,
    ReqCompActivationMethodPos = 4,
    CompLocationOffsetPos = 5,
    CompSizePos = 6,
    CompVersionPos = 7,
};

} // namespace fw_update

namespace pdr
{

using EID = uint8_t;
using TerminusHandle = uint16_t;
using TerminusID = uint8_t;
using SensorID = uint16_t;
using EntityType = uint16_t;
using EntityInstance = uint16_t;
using ContainerID = uint16_t;
using StateSetId = uint16_t;
using CompositeCount = uint8_t;
using SensorOffset = uint8_t;
using EventState = uint8_t;
using TerminusValidity = uint8_t;

//!< Subset of the State Set that is supported by a effecter/sensor
using PossibleStates = std::set<uint8_t>;
//!< Subset of the State Set that is supported by each effecter/sensor in a
//!< composite efffecter/sensor
using CompositeSensorStates = std::vector<PossibleStates>;
using EntityInfo = std::tuple<ContainerID, EntityType, EntityInstance>;
using SensorInfo = std::tuple<EntityInfo, CompositeSensorStates>;

} // namespace pdr

} // namespace pldm
