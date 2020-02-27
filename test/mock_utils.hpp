#include "libpldmresponder/bios_table.hpp"

#include <gmock/gmock.h>

using namespace pldm::responder::bios;

class MockBIOSStringTable : public BIOSStringTable
{
  public:
    MockBIOSStringTable() : BIOSStringTable({})
    {
    }

    MOCK_METHOD(uint16_t, findHandle, (const std::string&), (const override));
};