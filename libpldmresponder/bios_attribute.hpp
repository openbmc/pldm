#pragma once

#include "bios_table.hpp"
#include "utils.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "bios_table.h"

namespace pldm
{
namespace responder
{
namespace bios
{

using Json = nlohmann::json;
using namespace pldm::utils;

/** @struct DbusMapping
 *  @brief Dbus backend for BIOS attribute
 */
struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
    std::string propertyType; //!< D-Bus property type
};

/** @class BIOSAttribute
 *  @brief Provide interfaces to implement specific types of attributes
 */
class BIOSAttribute
{
  public:
    /** @brief Construct a bios attribute
     *  @param[in] entry - Json Object
     */
    BIOSAttribute(const Json& entry, DBusHandler* const dbusHandler);

    /** Virtual destructor
     */
    virtual ~BIOSAttribute()
    {
    }

    /** @brief Set Attribute value On Dbus according to the attribute value
     * entry
     *  @param[in] attrValueEntry - The attribute value entry
     *  @param[in] attrEntry - The attribute entry corresponding to the
     *                         attribute value entry
     *  @param[in] stringTable - The string table
     */
    virtual void
        setAttrValueOnDbus(const pldm_bios_attr_val_table_entry* attrValueEntry,
                           const pldm_bios_attr_table_entry* attrEntry,
                           const BIOSStringTable& stringTable) = 0;

    /** @brief Construct corresponding entries at the end of the attribute table
     *         and attribute value tables
     *  @param[in] stringTable - The string Table
     *  @param[in,out] attrTable - The attribute table
     *  @param[in,out] attrValueTable - The attribute value table
     */
    virtual void constructEntry(const BIOSStringTable& stringTable,
                                Table& attrTable, Table& attrValueTable) = 0;

    /** @brief Name of this attribute */
    const std::string name;

    /** Weather this attribute is read-only */
    const bool readOnly;

  protected:
    /** @brief dbus backend, nullopt if this attribute is read-only*/
    std::optional<DBusMapping> dBusMap;
    DBusHandler* const dbusHandler;
};

} // namespace bios
} // namespace responder
} // namespace pldm