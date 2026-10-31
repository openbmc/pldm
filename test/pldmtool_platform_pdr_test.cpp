#include "pldmtool/pldm_cmd_helper.hpp"
#include "pldmtool/pldm_platform_cmd.hpp"

#include <libpldm/base.h>
#include <libpldm/entity.h>
#include <libpldm/platform.h>

#include <CLI/CLI.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{

std::vector<uint8_t> gPendingResponse;

} // namespace

extern "C" int pldm_instance_db_init_default(struct pldm_instance_db** /*ctx*/)
{
    return 0;
}

extern "C" int pldm_instance_db_destroy(struct pldm_instance_db* /*ctx*/)
{
    return 0;
}

namespace pldmtool::helper
{

using ordered_json = nlohmann::ordered_json;

void fillCompletionCode(uint8_t /*completionCode*/, ordered_json& /*data*/,
                        uint8_t /*pldmType*/)
{}

void CommandInterface::exec()
{
    if (getCommandName() != "getPDR" || gPendingResponse.empty())
    {
        return;
    }
    auto* msg = reinterpret_cast<pldm_msg*>(gPendingResponse.data());
    parseResponseMsg(msg, gPendingResponse.size() - sizeof(pldm_msg_hdr));
}

int CommandInterface::pldmSendRecv(std::vector<uint8_t>& /*requestMsg*/,
                                   std::vector<uint8_t>& /*responseMsg*/)
{
    return PLDM_SUCCESS;
}

} // namespace pldmtool::helper

namespace
{

using ordered_json = nlohmann::ordered_json;

// Build an entity-association PDR record of `recordBytes` bytes with the
// given num_children field and `children.size()` actual child entities.
std::vector<uint8_t> makeEntityAssocRecord(
    size_t recordBytes, uint8_t numChildren,
    const std::vector<pldm_entity>& children)
{
    constexpr size_t fixedPrefixSize =
        sizeof(pldm_pdr_hdr) + offsetof(pldm_pdr_entity_association, children);
    const size_t minNeeded =
        fixedPrefixSize + children.size() * sizeof(pldm_entity);
    std::vector<uint8_t> buf(std::max(recordBytes, minNeeded), 0);

    auto* hdr = reinterpret_cast<pldm_pdr_hdr*>(buf.data());
    hdr->record_handle = 1;
    hdr->version = 1;
    hdr->type = PLDM_PDR_ENTITY_ASSOCIATION;
    hdr->record_change_num = 0;
    hdr->length = static_cast<uint16_t>(
        buf.size() >= sizeof(pldm_pdr_hdr) ? buf.size() - sizeof(pldm_pdr_hdr)
                                           : 0);

    if (buf.size() >= fixedPrefixSize)
    {
        auto* assoc = reinterpret_cast<pldm_pdr_entity_association*>(
            buf.data() + sizeof(pldm_pdr_hdr));
        assoc->container_id = 10;
        assoc->association_type = PLDM_ENTITY_ASSOCIAION_PHYSICAL;
        assoc->container = {PLDM_ENTITY_SYS_BOARD, 1, 0};
        assoc->num_children = numChildren;
        if (!children.empty())
        {
            std::memcpy(assoc->children, children.data(),
                        children.size() * sizeof(pldm_entity));
        }
    }

    buf.resize(recordBytes);
    return buf;
}

ordered_json runGetPdrParseResponse(const std::vector<uint8_t>& record)
{
    static CLI::App app;
    static const bool registered = []() {
        pldmtool::platform::registerCommand(app);
        return true;
    }();
    (void)registered;

    gPendingResponse.assign(
        sizeof(pldm_msg_hdr) + PLDM_GET_PDR_MIN_RESP_BYTES + record.size(), 0);
    auto* msg = reinterpret_cast<pldm_msg*>(gPendingResponse.data());
    EXPECT_EQ(PLDM_SUCCESS,
              encode_get_pdr_resp(
                  0, PLDM_SUCCESS, 0, 0, PLDM_PLATFORM_TRANSFER_START_AND_END,
                  static_cast<uint16_t>(record.size()), record.data(), 0, msg));

    const char* argv[] = {"pldmtool", "platform", "GetPDR", "-d", "1"};
    app.clear();
    testing::internal::CaptureStdout();
    app.parse(5, argv);
    std::string out = testing::internal::GetCapturedStdout();

    const auto jsonStart = out.find('{');
    EXPECT_NE(std::string::npos, jsonStart);
    return ordered_json::parse(out.substr(jsonStart));
}

TEST(PldmtoolPlatformPdrTest, ValidEntityAssociationPdrIsFormatted)
{
    const std::vector<pldm_entity> children = {
        {PLDM_ENTITY_PROC, 1, 10},
        {PLDM_ENTITY_PROC, 2, 10},
    };
    constexpr size_t recordSize =
        sizeof(pldm_pdr_hdr) + offsetof(pldm_pdr_entity_association, children) +
        2 * sizeof(pldm_entity);
    auto record = makeEntityAssocRecord(recordSize, 2, children);

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(10, output["containerID"]);
    EXPECT_EQ("Physical", output["associationType"]);
    EXPECT_EQ(2U, output["containedEntityCount"]);
    EXPECT_EQ("[Physical] Processor", output["containedEntityType[1]"]);
    EXPECT_EQ(1U, output["containedEntityInstanceNumber[1]"]);
    EXPECT_EQ(10U, output["containedEntityContainerID[1]"]);
    EXPECT_EQ("[Physical] Processor", output["containedEntityType[2]"]);
    EXPECT_EQ(2U, output["containedEntityInstanceNumber[2]"]);
    EXPECT_EQ(10U, output["containedEntityContainerID[2]"]);
    EXPECT_FALSE(output.contains("containedEntityInstanceNumber[3]"));
}

TEST(PldmtoolPlatformPdrTest, EntityAssociationPdrWithZeroChildrenIsFormatted)
{
    constexpr size_t recordSize =
        sizeof(pldm_pdr_hdr) + offsetof(pldm_pdr_entity_association, children);
    auto record = makeEntityAssocRecord(recordSize, 0, {});

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(10, output["containerID"]);
    EXPECT_EQ("Physical", output["associationType"]);
    EXPECT_EQ(0U, output["containedEntityCount"]);
    EXPECT_FALSE(output.contains("containedEntityType[1]"));
}

TEST(PldmtoolPlatformPdrTest, TruncatedEntityAssociationPrefixIsRejected)
{
    constexpr size_t truncatedSize =
        sizeof(pldm_pdr_hdr) + offsetof(pldm_pdr_entity_association, children) -
        1;
    auto record = makeEntityAssocRecord(truncatedSize, 0, {});

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(truncatedSize, output["responseCount"]);
    EXPECT_FALSE(output.contains("containerID"));
    EXPECT_FALSE(output.contains("containedEntityCount"));
}

TEST(PldmtoolPlatformPdrTest,
     EntityAssociationNumChildrenExceedingBufferByOneIsRejected)
{
    const std::vector<pldm_entity> children = {{PLDM_ENTITY_PROC, 1, 10}};
    constexpr size_t oneChildSize =
        sizeof(pldm_pdr_hdr) + offsetof(pldm_pdr_entity_association, children) +
        sizeof(pldm_entity);
    // Buffer only holds 1 child (26 bytes), but num_children claims 2.
    auto record = makeEntityAssocRecord(oneChildSize, 2, children);

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(oneChildSize, output["responseCount"]);
    EXPECT_FALSE(output.contains("containedEntityCount"));
    EXPECT_FALSE(output.contains("containedEntityType[1]"));
    EXPECT_FALSE(output.contains("containedEntityType[2]"));
}

TEST(PldmtoolPlatformPdrTest,
     EntityAssociationNumChildrenFarExceedingBufferIsRejected)
{
    const std::vector<pldm_entity> children = {{PLDM_ENTITY_PROC, 1, 10}};
    constexpr size_t oneChildSize =
        sizeof(pldm_pdr_hdr) + offsetof(pldm_pdr_entity_association, children) +
        sizeof(pldm_entity);
    // Buffer holds 1 child (26 bytes), but num_children claims 255.
    auto record = makeEntityAssocRecord(oneChildSize, 255, children);

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(oneChildSize, output["responseCount"]);
    EXPECT_FALSE(output.contains("containedEntityCount"));
    EXPECT_FALSE(output.contains("containedEntityType[2]"));
}

} // namespace
