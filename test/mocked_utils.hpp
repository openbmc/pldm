#include "utils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::utils;

class MockdBusHandler : public DBusHandler
{
  public:
    MOCK_CONST_METHOD2(setDbusProperty,
                       void(const DBusMapping&, const PropertyValue&));
    MOCK_METHOD(PropertyValue, getDbusPropertyVariant,
                (const char*, const char*, const char*), (const override));
};
