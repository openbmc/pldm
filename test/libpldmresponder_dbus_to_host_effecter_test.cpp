#include "dbus_to_host_effecters.hpp"
#include "mocked_utils.hpp"
#include "utils.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

using namespace pldm::host_effecters;
using namespace pldm::utils;
using namespace pldm::dbus_api;

class MockHostEffecterParser : public HostEffecterParser
{
  public:
    MockHostEffecterParser(int fd, const pldm_pdr* repo,
                           DBusHandler* const dbusHandler) :
        HostEffecterParser(nullptr, fd, repo, dbusHandler)
    {
    }

    void setHostStateEffecter(
        size_t /*effecterInfoIndex*/,
        std::vector<set_effecter_state_field>& /*stateField*/,
        uint16_t /*effecterId*/) override
    {
    }

    void createHostEffecterMatch(const std::string& /*objectPath*/,
                                 const std::string& /*interface*/,
                                 size_t /*effecterInfoIndex*/,
                                 size_t /*dbusInfoIndex*/,
                                 uint16_t /*effecterId*/) override
    {
    }
    const std::vector<EffecterInfo>& gethostEffecterInfo()
    {
        return hostEffecterInfo;
    }
};

TEST(HostEffecterParser, parseEffecterJson)
{
    MockdBusHandler dbusHandler;
    int sockfd{};
    MockHostEffecterParser hostEffecterParser(sockfd, nullptr, &dbusHandler);
    // no host effecter json
    ASSERT_THROW(
        hostEffecterParser.parseEffecterJson("./host_effecter_jsons/no_json"),
        std::exception);
    // malformed host effecter json
    ASSERT_THROW(
        hostEffecterParser.parseEffecterJson("./host_effecter_jsons/malformed"),
        std::exception);

    hostEffecterParser.parseEffecterJson("./host_effecter_jsons/good");
    auto hostEffecterInfo = hostEffecterParser.gethostEffecterInfo();
    EXPECT_EQ(hostEffecterInfo.size(), 1);
    EXPECT_EQ(hostEffecterInfo[0].entityInstance, 0);
    EXPECT_EQ(hostEffecterInfo[0].entityType, 33);
    EXPECT_EQ(hostEffecterInfo[0].dbusInfo.size(), 1);
    DBusInfo dbusInfo{{"/xyz/openbmc_project/control/host0/boot",
                       "xyz.openbmc_project.Control.Boot.Mode", "BootMode",
                       "string"},
                      {"xyz.openbmc_project.Control.Boot.Mode.Modes.Regular"},
                      {196, {2}}};
    auto& temp = hostEffecterInfo[0].dbusInfo[0];
    EXPECT_EQ(temp.dbusMap.objectPath == dbusInfo.dbusMap.objectPath, true);
    EXPECT_EQ(temp.dbusMap.interface == dbusInfo.dbusMap.interface, true);
    EXPECT_EQ(temp.dbusMap.propertyName == dbusInfo.dbusMap.propertyName, true);
    EXPECT_EQ(temp.dbusMap.propertyType == dbusInfo.dbusMap.propertyType, true);
}

TEST(HostEffecterParser, findNewStateValue)
{
    MockdBusHandler dbusHandler;
    int sockfd{};
    MockHostEffecterParser hostEffecterParser(sockfd, nullptr, &dbusHandler);
    hostEffecterParser.parseEffecterJson("./host_effecter_jsons/good");

    std::string s1 = "xyz.openbmc_project.Control.Boot.Mode.Modes.Regular";
    std::string s2 = "xyz.openbmc_project.Control.Boot.Mode.Modes.Setup";
    PropertyValue val1 = s1;
    PropertyValue val2 = s2;
    auto newState = hostEffecterParser.findNewStateValue(0, 0, val1);
    EXPECT_EQ(newState, 2);

    ASSERT_THROW(hostEffecterParser.findNewStateValue(0, 0, val2),
                 std::exception);
}
