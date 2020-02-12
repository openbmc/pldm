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

TEST(BIOSAttribute, CtorTest)
{
    auto jsonReadOnly = R"({
      "attribute_name" : "ReadOnly"
    })"_json;

    TestAttribute readOnly{jsonReadOnly};
    EXPECT_EQ(readOnly.name, "ReadOnly");
    EXPECT_EQ(readOnly.readOnly, true);

    auto jsonReadOnlyError = R"({
      "attribute_nam":"ReadOnly"
    })"_json;
    using Json = nlohmann::json;

    EXPECT_THROW(TestAttribute{jsonReadOnlyError}, Json::exception);

    auto jsonReadWrite = R"({
      "attribute_name":"ReadWrite",
      "dbus":
           {
               "object_path" : "/xyz/abc/def",
               "interface" : "xyz.openbmc.FWBoot.Side",
               "property_name" : "Side",
               "property_type" : "bool"
           }
    })"_json;

    TestAttribute readWrite{jsonReadWrite};
    EXPECT_EQ(readWrite.name, "ReadWrite");
    EXPECT_EQ(readWrite.readOnly, false);

    auto jsonReadWriteError = R"({
      "attribute_name":"ReadWrite",
      "dbus":
           {
               "object_path" : "/xyz/abc/def",
               "interface" : "xyz.openbmc.FWBoot.Side",
               "property_name" : "Side"
           }
    })"_json; // property_type is needed.

    EXPECT_THROW(TestAttribute{jsonReadWriteError}, Json::exception);
}