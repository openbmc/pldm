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
    BIOSIntegerAttribute(const Json& entry);

    int setAttrValueOnDbus(const pldm_bios_attr_val_table_entry* attrValueEntry,
                           const pldm_bios_attr_table_entry* attrEntry,
                           const BIOSStringTable& stringTable) override;

    void constructEntry(const BIOSStringTable& stringTable, Table& attrTable,
                        Table& attrValueTable) override;

  private:
    uint64_t lowerBound;
    uint64_t upperBound;
    uint32_t scalarIncrement;
    uint64_t defaultValue;
    uint64_t getAttrValue();
    void updateDbusProperty(uint64_t value);
};

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm