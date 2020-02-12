#pragma once

#include "bios_table.hpp"

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
namespace wip
{

using Json = nlohmann::json;
using namespace pldm::responder::bios;

struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
    std::string propertyType; //!< D-Bus property type
};

class BIOSAttribute
{
  public:
    BIOSAttribute(const Json& entry);

    virtual ~BIOSAttribute()
    {
    }

    virtual void
        setAttrValueOnDbus(const pldm_bios_attr_val_table_entry* attrValueEntry,
                           const pldm_bios_attr_table_entry* attrEntry,
                           const BIOSStringTable& stringTable) = 0;

    virtual void constructEntry(const BIOSStringTable& stringTable,
                                Table& attrTable, Table& attrValueTable) = 0;

    const std::string name;
    const bool readOnly;

  protected:
    std::optional<DBusMapping> dBusMap;
};

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm