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
    EXPECT_EQ(manager.preCondition(1), "");
    EXPECT_EQ(manager.postCondition(1), "");
    EXPECT_EQ(manager.conditions(1), ConditionPaths());
}

TEST_F(ConditionCollectorTest, NonExistentFileReturnsEmpty)
{
    fs::path nonExistentPath = "/nonexistent/path/config.json";
    ConditionConfigManager manager(nonExistentPath);
    EXPECT_EQ(manager.preCondition(1), "");
    EXPECT_EQ(manager.postCondition(1), "");
}

TEST_F(ConditionCollectorTest, ValidConfigParsingPreCondition)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "ComIdentifier": 1,
                "PreUpdateTarget": "bic-pre-update",
                "PostUpdateTarget": "bic-post-update",
                "ArgTemplate": ["boardName"]
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition(1), "bic-pre-update");
    EXPECT_EQ(manager.postCondition(1), "bic-post-update");

    auto [pre, post] = manager.conditions(1);
    EXPECT_EQ(pre, "bic-pre-update");
    EXPECT_EQ(post, "bic-post-update");

    auto template_vec = manager.argTemplate(1);
    EXPECT_EQ(template_vec.size(), 1);
    EXPECT_EQ(template_vec[0], "boardName");
}

TEST_F(ConditionCollectorTest, EmptyConditionStrings)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "NIC",
                "ComIdentifier": 2,
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
    EXPECT_EQ(manager.preCondition(2), "");
    EXPECT_EQ(manager.postCondition(2), "");
}

TEST_F(ConditionCollectorTest, MultipleComponentsParsing)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIOS",
                "ComIdentifier": 3,
                "PreUpdateTarget": "bios-pre",
                "PostUpdateTarget": "bios-post"
            },
            {
                "Component": "BIOS_VR",
                "ComIdentifier": 4,
                "PreUpdateTarget": "",
                "PostUpdateTarget": "vr-post"
            },
            {
                "Component": "EC",
                "ComIdentifier": 5,
                "PreUpdateTarget": "ec-pre",
                "PostUpdateTarget": ""
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);

    EXPECT_EQ(manager.preCondition(3), "bios-pre");
    EXPECT_EQ(manager.postCondition(3), "bios-post");

    EXPECT_EQ(manager.preCondition(4), "");
    EXPECT_EQ(manager.postCondition(4), "vr-post");

    EXPECT_EQ(manager.preCondition(5), "ec-pre");
    EXPECT_EQ(manager.postCondition(5), "");
}

TEST_F(ConditionCollectorTest, NonExistentComponentReturnsEmpty)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "ComIdentifier": 1,
                "PreUpdateTarget": "bic-pre",
                "PostUpdateTarget": "bic-post"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition(0), "");
    EXPECT_EQ(manager.postCondition(0), "");
    EXPECT_EQ(manager.conditions(0), ConditionPaths());
}

TEST_F(ConditionCollectorTest, InvalidJsonFormat)
{
    const std::string configContent = R"({invalid json})";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    // Should not throw, just handle gracefully
    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition(1), "");
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
    EXPECT_EQ(manager.preCondition(1), "");
}

TEST_F(ConditionCollectorTest, MissingComponentNameField)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "PreUpdateTarget": "pre",
                "PostUpdateTarget": "post"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    // Should skip invalid entry and not crash
    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition(1), "");
}

TEST_F(ConditionCollectorTest, BothTargetFieldsAbsent)
{
    // Neither PreUpdateTarget nor PostUpdateTarget is present —
    // entry should be skipped and queries should return empty string
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "ComIdentifier": 1
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition(1), "");
    EXPECT_EQ(manager.postCondition(1), "");
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
                "ComIdentifier": 3,
                "PostUpdateTarget": "bios-post"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition(3), "");
    EXPECT_EQ(manager.postCondition(3), "bios-post");
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
                "ComIdentifier": 2,
                "PreUpdateTarget": "nic-pre"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition(2), "nic-pre");
    EXPECT_EQ(manager.postCondition(2), "");
}
