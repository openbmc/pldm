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

class BIOSEnumAttribute : public BIOSAttribute
{
  public:
    BIOSEnumAttribute(const Json& entry);

    void
        setAttrValueOnDbus(const pldm_bios_attr_val_table_entry* attrValueEntry,
                           const pldm_bios_attr_table_entry* attrEntry,
                           const BIOSStringTable& stringTable) override;

    void constructEntry(const BIOSStringTable& stringTable, Table& attrTable,
                        Table& attrValueTable) override;

  private:
    std::vector<std::string> possibleValues;
    std::string defaultValue;
    std::vector<uint8_t> getValueIndices(const std::string& value);
    std::vector<uint16_t>
        getPossibleValuesHandle(const BIOSStringTable& stringTable);

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

} // namespace bios
} // namespace responder
} // namespace pldm