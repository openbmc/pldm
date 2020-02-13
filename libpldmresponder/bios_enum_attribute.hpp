#pragma once

#include "bios_attribute.hpp"

#include <map>
#include <set>
#include <string>
#include <variant>

class TestBIOSEnumAttribute;

namespace pldm
{
namespace responder
{
namespace bios
{

/** @class BIOSEnumAttribute
 *  @brief Associate enum entry(attr table and attribute value table) and
 *         dbus attribute
 */
class BIOSEnumAttribute : public BIOSAttribute
{
  public:
    friend class ::TestBIOSEnumAttribute;
    /** @brief Construct a bios enum attribute
     *  @param[in] entry - Json Object
     *  @param[in] dbusHandler - Dbus Handler
     */
    BIOSEnumAttribute(const Json& entry, DBusHandler* const dbusHandler);

    /** @brief Set Attribute value On Dbus according to the attribute value
     *         entry
     *  @param[in] attrValueEntry - The attribute value entry
     *  @param[in] attrEntry - The attribute entry corresponding to the
     *                         attribute value entry
     *  @param[in] stringTable - The string table
     */
    void
        setAttrValueOnDbus(const pldm_bios_attr_val_table_entry* attrValueEntry,
                           const pldm_bios_attr_table_entry* attrEntry,
                           const BIOSStringTable& stringTable) override;

    /** @brief Construct corresponding entries at the end of the attribute table
     *         and attribute value tables
     *  @param[in] stringTable - The string Table
     *  @param[in,out] attrTable - The attribute table
     *  @param[in,out] attrValueTable - The attribute value table
     */
    void constructEntry(const BIOSStringTable& stringTable, Table& attrTable,
                        Table& attrValueTable) override;

  private:
    std::vector<std::string> possibleValues;
    std::string defaultValue;
    std::vector<uint8_t> getValueIndices(const std::string& value,
                                         const std::vector<std::string>& pVs);
    std::vector<uint16_t>
        getPossibleValuesHandle(const BIOSStringTable& stringTable,
                                const std::vector<std::string>& pVs);

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