#include "libpldmresponder/bios_integer_attribute.hpp"

#include <memory>
#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::responder::bios;
using testing::ElementsAreArray;

class TestBIOSIntegerAttribute : public ::testing::Test
{
  public:
    const BIOSAttrTable::IntegerField&
        getIntegerInfo(const BIOSIntegerAttribute& attribute)
    {
        return attribute.integerInfo;
    }
};

TEST_F(TestBIOSIntegerAttribute, CtorTest)
{
   
}