#pragma once

#include <stdint.h>

#include <bitset>
#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace pldm
{

using eid = uint8_t;
using Request = std::vector<uint8_t>;
using Response = std::vector<uint8_t>;

namespace dbus
{

using ObjectPath = std::string;
using Service = std::string;
using Interface = std::string;
using Interfaces = std::vector<std::string>;
using Property = std::string;
using PropertyType = std::string;
using Value = std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
                           int64_t, uint64_t, double, std::string>;

} // namespace dbus

namespace fw_update
{

using DescriptorType = uint16_t;
using DescriptorData = std::vector<uint8_t>;
using VendorDefinedDescriptorTitle = std::string;
using VendorDefinedDescriptorData = std::vector<uint8_t>;
using VendorDefinedDescriptorInfo =
    std::tuple<VendorDefinedDescriptorTitle, VendorDefinedDescriptorData>;

using Descriptors =
    std::map<DescriptorType,
             std::variant<DescriptorData, VendorDefinedDescriptorInfo>>;
using DescriptorMap = std::unordered_map<eid, Descriptors>;

using CompClassification = uint16_t;
using CompIdentifier = uint16_t;
using CompKey = std::pair<CompClassification, CompIdentifier>;
using CompClassificationIndex = uint8_t;
using ComponentInfo  = std::map<CompKey, CompClassificationIndex>;
using ComponentInfoMap = std::unordered_map<eid, ComponentInfo>;

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

//!< Subset of the State Set that is supported by a effecter/sensor
using PossibleStates = std::set<uint8_t>;
//!< Subset of the State Set that is supported by each effecter/sensor in a
//!< composite efffecter/sensor
using CompositeSensorStates = std::vector<PossibleStates>;
using EntityInfo = std::tuple<ContainerID, EntityType, EntityInstance>;
using SensorInfo = std::tuple<EntityInfo, CompositeSensorStates>;

} // namespace pdr

} // namespace pldm
