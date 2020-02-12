#include "libpldmresponder/bios/attribute.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

using namespace pldm::responder::bios;

class TestAttribute : public BIOSAttribute
{
  public:
    TestAttribute(const Json& entry) : BIOSAttribute(entry)
    {
    }

    int setAttrValueOnDbus(const pldm_bios_attr_val_table_entry*,
                           const pldm_bios_attr_table_entry*,
                           const BIOSStringTable&) override
    {
        return 0;
    }

    void constructEntry(const BIOSStringTable&, Table&, Table&)
    {
    }
};

TEST(ReadOnly, testSuccess)
{
    auto j = R"({
         "attribute_name" : "CodeUpdatePolicy",
         "possible_values" : [ "Concurrent", "Disruptive" ],
         "default_values" : [ "Concurrent" ]
      })"_json;
    TestAttribute testAttribute(j);

    EXPECT_EQ(testAttribute.name, "CodeUpdatePolicy");
    EXPECT_EQ(testAttribute.readOnly, true);
}