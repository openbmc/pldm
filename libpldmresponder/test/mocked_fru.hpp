#include "libpldmresponder/fru.hpp"

#include <libpldm/pdr.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::ElementsAreArray;
using namespace pldm::responder;
class MockFruImpl : public FruImpl
{
  public:
    using FruImpl::FruImpl;
    MOCK_METHOD1(
        getEntityByObjectPath,
        std::optional<pldm_entity>(const pldm::responder::dbus::InterfaceMap&));
};
