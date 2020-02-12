#pragma once
#include "attribute.hpp"

#include <string>

namespace pldm
{
namespace responder
{
namespace bios
{
namespace wip
{

class BIOSStringAttribute : public BIOSAttribute
{
  public:
    enum BIOSStringEncoding
    {
        UNKNOWN = 0x00,
        ASCII = 0x01,
        HEX = 0x02,
        UTF_8 = 0x03,
        UTF_16LE = 0x04,
        UTF_16BE = 0x05,
        VENDOR_SPECIFIC = 0xFF
    };

    inline static const std::map<std::string, uint8_t> strTypeMap{
        {"Unknown", UNKNOWN},
        {"ASCII", ASCII},
        {"Hex", HEX},
        {"UTF-8", UTF_8},
        {"UTF-16LE", UTF_16LE},
        {"UTF-16LE", UTF_16LE},
        {"Vendor Specific", VENDOR_SPECIFIC}};

    BIOSStringAttribute(const Json& entry);

    int setAttrValueOnDbus(const pldm_bios_attr_val_table_entry* attrValueEntry,
                           const pldm_bios_attr_table_entry* attrEntry,
                           const BIOSStringTable& stringTable) override;

    void constructEntry(const BIOSStringTable& stringTable, Table& attrTable,
                        Table& attrValueTable) override;

  private:
    uint8_t stringType;
    uint16_t minStringLength;
    uint16_t maxStringLength;
    uint16_t defaultStringLength;
    std::string defaultString;

    std::string stringToUtf8(BIOSStringEncoding stringType,
                             const std::vector<uint8_t>& data);

    std::string getAttrValue();
};

} // namespace wip
} // namespace bios
} // namespace responder
} // namespace pldm
