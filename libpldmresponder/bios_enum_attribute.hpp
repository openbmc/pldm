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
    BIOSEnumAttribute(const Json& entry, const BIOSStringTable& stringTable,
                      DBusHandler* const dbusHandler);

    /** @brief Set Attribute value On Dbus according to the attribute value
     *         entry
     *  @param[in] attrValueEntry - The attribute value entry
     */
    void setAttrValueOnDbus(
        const pldm_bios_attr_val_table_entry* attrValueEntry) override;

    /** @brief Construct corresponding entries at the end of the attribute table
     *         and attribute value tables
     *  @param[in] stringTable - The string Table
     *  @param[in,out] attrTable - The attribute table
     *  @param[in,out] attrValueTable - The attribute value table
     */
    void constructEntry(Table& attrTable, Table& attrValueTable) override;

    int updateAttrVal(Table& newValue, const PropertyValue& newPropVal);

  private:
    /** @brief Map of stringHandles and strings */
    std::map<std::string, uint16_t> strings;
    std::vector<std::string> possibleValues;
    std::string defaultValue;

    /** @brief Get index of the given value in possible values
     *  @param[in] value - The given value
     *  @param[in] pVs - The possible values
     *  @return Index of the given value in possible values
     */
    uint8_t getValueIndex(const std::string& value);

    /** @brief Get handles of possible values
     *  @param[in] stringTable - The bios string table
     *  @param[in] pVs - The possible values
     *  @return The handles
     */
    std::vector<uint16_t> getPossibleValuesHandle();

    using ValMap = std::map<PropertyValue, std::string>;

    /** @brief Map of value on dbus and pldm */
    ValMap valMap;

    /** @brief Build the map of dbus value to pldm enum value
     *  @param[in] dbusVals - The dbus values in the json's dbus section
     */
    void buildValMap(const Json& dbusVals);

    /** @brief Get index of the current value in possible values
     *  @return The index of the current value in possible values
     */
    uint8_t getAttrValueIndex();
};

} // namespace bios
} // namespace responder
} // namespace pldm
