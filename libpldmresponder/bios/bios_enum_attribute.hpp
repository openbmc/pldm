#pragma once

#include "bios_attribute.hpp"

#include <map>
#include <set>
#include <string>
#include <variant>

namespace pldm
{
namespace responder
{
namespace bios
{
namespace wip
{

class BIOSEnumAttribute : public BIOSAttribute
{
  public:
    BIOSEnumAttribute(const Json& entry, const BIOSStringTable& stringTable);

    void
        setAttrValueOnDbus(const pldm_bios_attr_val_table_entry* attrValueEntry,
                           const pldm_bios_attr_table_entry* attrEntry,
                           const BIOSStringTable& stringTable) override;

    void constructEntry(const BIOSStringTable& stringTable, Table& attrTable,
                        Table& attrValueTable) override;

  private:
    pldm_bios_table_attr_entry_enum_info enumEntryInfo;
    std::vector<uint16_t> possibleValuesHandle;
    std::vector<uint8_t> defaultIndices;

    std::vector<std::string> possibleValues;
    std::string defaultValue;
    std::vector<uint8_t> findValueIndex(const std::string value);
    void buildEntryInfo(const BIOSStringTable& stringTable);

    using PropertyValue =
        std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
                     int64_t, uint64_t, double, std::string>;
    inline static const std::set<std::string> supportedDbusPropertyTypes = {
        "bool",     "uint8_t", "int16_t",  "uint16_t", "int32_t",
        "uint32_t", "int64_t", "uint64_t", "double",   "string"};

    using ValMap = std::map<PropertyValue, std::string>;
    ValMap valMap;
    void buildValMap(const Json& dbusVals);
    void updateDbusProperty(const PropertyValue& value);

    std::vector<uint8_t> getAttrValue();
};

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm