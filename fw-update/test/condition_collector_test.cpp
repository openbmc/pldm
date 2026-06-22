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
        testConfigPath =
            fs::temp_directory_path() / "test_condition_config.json";
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
    EXPECT_EQ(manager.preCondition("AnyComponent"), "");
    EXPECT_EQ(manager.postCondition("AnyComponent"), "");
    EXPECT_EQ(manager.conditions("AnyComponent"), ConditionPaths());
}

TEST_F(ConditionCollectorTest, NonExistentFileReturnsEmpty)
{
    fs::path nonExistentPath = "/nonexistent/path/config.json";
    ConditionConfigManager manager(nonExistentPath);
    EXPECT_EQ(manager.preCondition("AnyComponent"), "");
    EXPECT_EQ(manager.postCondition("AnyComponent"), "");
}

TEST_F(ConditionCollectorTest, ValidConfigParsingPreCondition)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
                "PreUpdateTarget": "bic-pre-update",
                "PostUpdateTarget": "bic-post-update"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition("BIC"), "bic-pre-update");
    EXPECT_EQ(manager.postCondition("BIC"), "bic-post-update");

    auto [pre, post] = manager.conditions("BIC");
    EXPECT_EQ(pre, "bic-pre-update");
    EXPECT_EQ(post, "bic-post-update");
}

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
                "PreUpdateTarget": "bios-pre",
                "PostUpdateTarget": "bios-post"
            },
            {
                "Component": "BIOS_VR",
                "PreUpdateTarget": "",
                "PostUpdateTarget": "vr-post"
            },
            {
                "Component": "EC",
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

    EXPECT_EQ(manager.preCondition("BIOS"), "bios-pre");
    EXPECT_EQ(manager.postCondition("BIOS"), "bios-post");

    EXPECT_EQ(manager.preCondition("BIOS_VR"), "");
    EXPECT_EQ(manager.postCondition("BIOS_VR"), "vr-post");

    EXPECT_EQ(manager.preCondition("EC"), "ec-pre");
    EXPECT_EQ(manager.postCondition("EC"), "");
}

TEST_F(ConditionCollectorTest, NonExistentComponentReturnsEmpty)
{
    const std::string configContent = R"(
    {
        "Components": [
            {
                "Component": "BIC",
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
    EXPECT_EQ(manager.preCondition("NonExistent"), "");
    EXPECT_EQ(manager.postCondition("NonExistent"), "");
    EXPECT_EQ(manager.conditions("NonExistent"), ConditionPaths());
}

TEST_F(ConditionCollectorTest, InvalidJsonFormat)
{
    const std::string configContent = R"({invalid json})";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    // Should not throw, just handle gracefully
    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition("AnyComponent"), "");
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
    EXPECT_EQ(manager.preCondition("AnyComponent"), "");
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
    EXPECT_EQ(manager.preCondition("AnyComponent"), "");
}

TEST_F(ConditionCollectorTest, BothTargetFieldsAbsent)
{
    // Neither PreUpdateTarget nor PostUpdateTarget is present —
    // both should default to empty string
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
                "PostUpdateTarget": "bios-post"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition("BIOS"), "");
    EXPECT_EQ(manager.postCondition("BIOS"), "bios-post");
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
                "PreUpdateTarget": "nic-pre"
            }
        ]
    }
    )";

    std::ofstream configFile(testConfigPath);
    configFile << configContent;
    configFile.close();

    ConditionConfigManager manager(testConfigPath);
    EXPECT_EQ(manager.preCondition("NIC"), "nic-pre");
    EXPECT_EQ(manager.postCondition("NIC"), "");
}
