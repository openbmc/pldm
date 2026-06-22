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

TEST_F(ConditionCollectorTest, EmptyPathReturnsEmpty)
{
    ConditionConfigManager manager("");
    EXPECT_EQ(manager.preCondition("BIC"), "");
    EXPECT_EQ(manager.postCondition("BIC"), "");
    EXPECT_EQ(manager.conditions("BIC"), ConditionPaths());
}

TEST_F(ConditionCollectorTest, NonExistentFileReturnsEmpty)
{
    fs::path nonExistentPath = "/nonexistent/path/config.json";
    ConditionConfigManager manager(nonExistentPath);
    EXPECT_EQ(manager.preCondition("BIC"), "");
    EXPECT_EQ(manager.postCondition("BIC"), "");
}

TEST_F(ConditionCollectorTest, ValidConfigParsingPreCondition)
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
    EXPECT_EQ(manager.preCondition("BIC"), "bic-pre-update.service");
    EXPECT_EQ(manager.postCondition("BIC"), "bic-post-update@.service");

    auto [pre, post] = manager.conditions("BIC");
    EXPECT_EQ(pre, "bic-pre-update.service");
    EXPECT_EQ(post, "bic-post-update@.service");
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
    EXPECT_EQ(manager.preCondition("NIC"), "");
    EXPECT_EQ(manager.postCondition("NIC"), "");
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

    EXPECT_EQ(manager.preCondition("BIOS"), "bios-pre.service");
    EXPECT_EQ(manager.postCondition("BIOS"), "bios-post.service");

    EXPECT_EQ(manager.preCondition("BIOS_VR"), "");
    EXPECT_EQ(manager.postCondition("BIOS_VR"), "vr-post.service");

    EXPECT_EQ(manager.preCondition("EC"), "ec-pre.service");
    EXPECT_EQ(manager.postCondition("EC"), "");
}

TEST_F(ConditionCollectorTest, NonExistentComponentReturnsEmpty)
{
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

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition("UNKNOWN"), "");
    EXPECT_EQ(manager.postCondition("UNKNOWN"), "");
    EXPECT_EQ(manager.conditions("UNKNOWN"), ConditionPaths());
}

TEST_F(ConditionCollectorTest, InvalidJsonFormat)
{
    const std::string configContent = R"({invalid json})";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    // Should not throw, just handle gracefully
    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition("BIC"), "");
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
    EXPECT_EQ(manager.preCondition("BIC"), "");
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
    EXPECT_EQ(manager.preCondition("BIC"), "");
}

TEST_F(ConditionCollectorTest, BothTargetFieldsAbsent)
{
    // Neither PreUpdateTarget nor PostUpdateTarget is present —
    // entry should be skipped and queries should return empty string
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
    EXPECT_EQ(manager.preCondition("BIC"), "");
    EXPECT_EQ(manager.postCondition("BIC"), "");
}

TEST_F(ConditionCollectorTest, OnlyPreTargetAbsent)
{
    // PostUpdateTarget is present but PreUpdateTarget is absent —
    // pre should default to empty string, post should be populated
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
    EXPECT_EQ(manager.preCondition("BIOS"), "");
    EXPECT_EQ(manager.postCondition("BIOS"), "bios-post.service");
}

TEST_F(ConditionCollectorTest, OnlyPostTargetAbsent)
{
    // PreUpdateTarget is present but PostUpdateTarget is absent —
    // post should default to empty string, pre should be populated
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
    EXPECT_EQ(manager.preCondition("NIC"), "nic-pre.service");
    EXPECT_EQ(manager.postCondition("NIC"), "");
}
