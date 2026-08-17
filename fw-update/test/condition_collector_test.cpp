#include "fw-update/condition_collector.hpp"

#include <filesystem>
#include <fstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::fw_update;
namespace fs = std::filesystem;

class ConditionCollectorTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        testConfigPath = "test_condition_config.json";
    }

    void TearDown() override
    {
        if (fs::exists(testConfigPath))
        {
            fs::remove(testConfigPath);
        }
    }

    fs::path testConfigPath;
};

TEST_F(ConditionCollectorTest, UnconfiguredLookupReturnsDefault)
{
    ConditionConfigManager emptyPathManager("");
    EXPECT_EQ(emptyPathManager.getCondition("BIC"), ComponentCondition());

    ConditionConfigManager missingFileManager("/nonexistent/path/config.json");
    EXPECT_EQ(missingFileManager.getCondition("BIC"), ComponentCondition());

    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "PreUpdateTarget": "bic-pre.service",
                "PostUpdateTarget": "bic-post.service"
            }
        ]
    }
    )";
    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager loadedManager(testConfigPath);
    EXPECT_EQ(loadedManager.getCondition("UNKNOWN"), ComponentCondition());
}

TEST_F(ConditionCollectorTest, ValidConfigParsing)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "PreUpdateTarget": "bic-pre-update.service",
                "PostUpdateTarget": "bic-post-update@.service"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    ComponentCondition condition = manager.getCondition("BIC");
    EXPECT_EQ(condition.preUpdateTarget, "bic-pre-update.service");
    EXPECT_EQ(condition.postUpdateTarget, "bic-post-update@.service");
    EXPECT_FALSE(condition.stopSensorPolling);
}

TEST_F(ConditionCollectorTest, GenerateArgFromBoardName)
{
    EXPECT_EQ(generateArg("board1"), "boardName=board1");
    EXPECT_EQ(generateArg(""), "");
}

// The schema requires a non-empty unit name and omission of the property to
// skip a condition. An empty name is still tolerated at runtime and reported as
// no condition.
TEST_F(ConditionCollectorTest, EmptyConditionStrings)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "NIC",
                "PreUpdateTarget": "",
                "PostUpdateTarget": ""
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    ComponentCondition condition = manager.getCondition("NIC");
    EXPECT_EQ(condition.preUpdateTarget, "");
    EXPECT_EQ(condition.postUpdateTarget, "");
}

TEST_F(ConditionCollectorTest, MultipleComponentsParsing)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIOS",
                "PreUpdateTarget": "bios-pre.service",
                "PostUpdateTarget": "bios-post.service"
            },
            {
                "Component": "BIOS_VR",
                "PostUpdateTarget": "vr-post.service"
            },
            {
                "Component": "EC",
                "PreUpdateTarget": "ec-pre.service"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);

    ComponentCondition bios = manager.getCondition("BIOS");
    EXPECT_EQ(bios.preUpdateTarget, "bios-pre.service");
    EXPECT_EQ(bios.postUpdateTarget, "bios-post.service");

    ComponentCondition biosVr = manager.getCondition("BIOS_VR");
    EXPECT_FALSE(biosVr.preUpdateTarget);
    EXPECT_EQ(biosVr.postUpdateTarget, "vr-post.service");

    ComponentCondition ec = manager.getCondition("EC");
    EXPECT_EQ(ec.preUpdateTarget, "ec-pre.service");
    EXPECT_FALSE(ec.postUpdateTarget);
}

TEST_F(ConditionCollectorTest, InvalidJsonFormat)
{
    const std::string configContent = R"({invalid json})";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    // Should not throw, just handle gracefully
    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.getCondition("BIC"), ComponentCondition());
}

TEST_F(ConditionCollectorTest, MissingComponentsKey)
{
    const std::string configContent = R"(
    {
        "NotComponents": []
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.getCondition("BIC"), ComponentCondition());
}

TEST_F(ConditionCollectorTest, MissingComponentNameField)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "PreUpdateTarget": "pre.service",
                "PostUpdateTarget": "post.service"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    // Should skip invalid entry and not crash
    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.getCondition("BIC"), ComponentCondition());
}

TEST_F(ConditionCollectorTest, BothTargetFieldsAbsent)
{
    // Neither PreUpdateTarget nor PostUpdateTarget is present —
    // both should come back unset, not just empty
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    ComponentCondition condition = manager.getCondition("BIC");
    EXPECT_FALSE(condition.preUpdateTarget);
    EXPECT_FALSE(condition.postUpdateTarget);
}

TEST_F(ConditionCollectorTest, OnlyPreTargetAbsent)
{
    // PostUpdateTarget is present but PreUpdateTarget is absent —
    // pre should come back unset, post should be populated
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIOS",
                "PostUpdateTarget": "bios-post.service"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    ComponentCondition condition = manager.getCondition("BIOS");
    EXPECT_FALSE(condition.preUpdateTarget);
    EXPECT_EQ(condition.postUpdateTarget, "bios-post.service");
}

TEST_F(ConditionCollectorTest, OnlyPostTargetAbsent)
{
    // PreUpdateTarget is present but PostUpdateTarget is absent —
    // post should come back unset, pre should be populated
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "NIC",
                "PreUpdateTarget": "nic-pre.service"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    ComponentCondition condition = manager.getCondition("NIC");
    EXPECT_EQ(condition.preUpdateTarget, "nic-pre.service");
    EXPECT_FALSE(condition.postUpdateTarget);
}

TEST_F(ConditionCollectorTest, StopSensorPollingTrueWithoutTargets)
{
    // A component may opt into stopping sensor polling without configuring
    // either a pre or post condition service.
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "StopSensorPolling": true
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    ComponentCondition condition = manager.getCondition("BIC");
    EXPECT_TRUE(condition.stopSensorPolling);
    EXPECT_FALSE(condition.preUpdateTarget);
    EXPECT_FALSE(condition.postUpdateTarget);
}

TEST_F(ConditionCollectorTest, StopSensorPollingFalseIsDefault)
{
    // Omitting the property defaults to false, same as an explicit false.
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "PreUpdateTarget": "bic-pre.service"
            },
            {
                "Component": "NIC",
                "PreUpdateTarget": "nic-pre.service",
                "StopSensorPolling": false
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_FALSE(manager.getCondition("BIC").stopSensorPolling);
    EXPECT_FALSE(manager.getCondition("NIC").stopSensorPolling);
}

TEST_F(ConditionCollectorTest, StopSensorPollingCombinedWithConditions)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "PreUpdateTarget": "bic-pre.service",
                "PostUpdateTarget": "bic-post.service",
                "StopSensorPolling": true
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    ComponentCondition condition = manager.getCondition("BIC");
    EXPECT_TRUE(condition.stopSensorPolling);
    EXPECT_EQ(condition.preUpdateTarget, "bic-pre.service");
    EXPECT_EQ(condition.postUpdateTarget, "bic-post.service");
}

TEST_F(ConditionCollectorTest, StopSensorPollingWrongTypeIsIgnored)
{
    // Ignore non-boolean StopSensorPolling values, treating them the same as
    // an absent property without throwing or crashing.
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "StopSensorPolling": "true"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_FALSE(manager.getCondition("BIC").stopSensorPolling);
}
