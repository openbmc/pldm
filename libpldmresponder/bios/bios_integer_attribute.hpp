#pragma once

#include "bios_attribute.hpp"

namespace pldm
{
namespace responder
{
namespace bios
{
namespace wip
{

class BIOSIntegerAttribute : public BIOSAttribute
{
  public:
    BIOSIntegerAttribute(const Json& entry, const BIOSStringTable& stringTable);

    void
        setAttrValueOnDbus(const pldm_bios_attr_val_table_entry* attrValueEntry,
                           const pldm_bios_attr_table_entry* attrEntry,
                           const BIOSStringTable& stringTable) override;

    void constructEntry(const BIOSStringTable& stringTable, Table& attrTable,
                        Table& attrValueTable) override;

  private:
    pldm_bios_table_attr_entry_integer_info integerEntryInfo;
    uint64_t getAttrValue();
    void updateDbusProperty(uint64_t value);
};

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm