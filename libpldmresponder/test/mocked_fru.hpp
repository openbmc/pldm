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
    MOCK_METHOD1(getEntityByObjectPath, std::optional<pldm_entity>(const pldm::responder::dbus::InterfaceMap&));
    //MOCK_METHOD((std::optional<pldm_entity>), getEntityByObjectPath, (const pldm::responder::dbus::InterfaceMap&));

};
/*
class MockFruParser : public pldm::responder::fru_parser::FruParser
{
  public:

    MOCK_METHOD(uint16_t, getEntityType, (std::string), (const, override));

};
    MockFruImpl(const std::string& configPath,
            const std::filesystem::path& fruMasterJsonPath, pldm_pdr* pdrRepo,
            pldm_entity_association_tree* entityTree,
            pldm_entity_association_tree* bmcEntityTree);
*/
