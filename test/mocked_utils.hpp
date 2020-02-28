#include "utils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::utils;

class MockdBusHandler : public DBusHandler
{
  public:
    MOCK_METHOD(void, setDbusProperty,
                (const DBusMapping&, const PropertyValue&), (const override));
};
