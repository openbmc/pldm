#include "common/utils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pldm
{
namespace utils
{
/** @brief helper function for parameter matching
 *  @param[in] lhs - left-hand side value
 *  @param[in] rhs - right-hand side value
 *  @return true if it matches
 */
inline bool operator==(const DBusMapping& lhs, const DBusMapping& rhs)
{
    return lhs.objectPath == rhs.objectPath && lhs.interface == rhs.interface &&
           lhs.propertyName == rhs.propertyName &&
           lhs.propertyType == rhs.propertyType;
}

} // namespace utils
} // namespace pldm

class MockdBusHandler : public pldm::utils::DBusHandler
{
  public:
    MOCK_METHOD(std::string, getService, (const char*, const char*),
                (const override));

    MOCK_METHOD(void, setDbusProperty,
                (const pldm::utils::DBusMapping&,
                 const pldm::utils::PropertyValue&),
                (const override));

    MOCK_METHOD(pldm::utils::PropertyValue, getDbusPropertyVariant,
                (const char*, const char*, const char*), (const override));

    MOCK_METHOD(pldm::utils::GetSubTreeResponse, getSubtree,
                (const std::string&, int, const std::vector<std::string>&),
                (const override));
};
