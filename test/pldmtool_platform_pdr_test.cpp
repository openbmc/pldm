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

void put16(std::vector<uint8_t>& v, uint16_t value)
{
    v.push_back(static_cast<uint8_t>(value & 0xff));
    v.push_back(static_cast<uint8_t>(value >> 8));
}

// Wrap `body` in a PDR header whose length field matches the bytes supplied.
std::vector<uint8_t> makeRecord(uint8_t type, const std::vector<uint8_t>& body)
{
    std::vector<uint8_t> record(sizeof(pldm_pdr_hdr), 0);
    auto* hdr = reinterpret_cast<pldm_pdr_hdr*>(record.data());
    hdr->record_handle = 1;
    hdr->version = 1;
    hdr->type = type;
    hdr->length = static_cast<uint16_t>(body.size());
    record.insert(record.end(), body.begin(), body.end());
    return record;
}

// One state_effecter_possible_states entry. `claimedSize` is the
// possible_states_size field, `suppliedBytes` is how many bit-field bytes
// actually follow it (the first one has bit 1 set).
std::vector<uint8_t> makePossibleStates(
    uint16_t stateSetId, uint8_t claimedSize, size_t suppliedBytes)
{
    std::vector<uint8_t> entry;
    put16(entry, stateSetId);
    entry.push_back(claimedSize);
    for (size_t i = 0; i < suppliedBytes; ++i)
    {
        entry.push_back(i == 0 ? 0x02 : 0x00);
    }
    return entry;
}

std::vector<uint8_t> makeStateEffecterRecord(uint8_t effecterInit,
                                             uint8_t compositeCount,
                                             const std::vector<uint8_t>& states)
{
    std::vector<uint8_t> body;
    put16(body, 1);                     // terminus_handle
    put16(body, 2);                     // effecter_id
    put16(body, PLDM_ENTITY_SYS_BOARD); // entity_type
    put16(body, 1);                     // entity_instance
    put16(body, 0);                     // container_id
    put16(body, 0);                     // effecter_semantic_id
    body.push_back(effecterInit);
    body.push_back(0);                  // has_description_pdr
    body.push_back(compositeCount);
    body.insert(body.end(), states.begin(), states.end());
    return makeRecord(PLDM_STATE_EFFECTER_PDR, body);
}

std::vector<uint8_t> concat(std::vector<uint8_t> a,
                            const std::vector<uint8_t>& b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

TEST(PldmtoolPlatformPdrTest, ValidStateEffecterPdrIsFormatted)
{
    auto record = makeStateEffecterRecord(
        0, 2, concat(makePossibleStates(1, 1, 1), makePossibleStates(2, 2, 2)));

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(2, output["effecterID"]);
    EXPECT_EQ("noInit", output["effecterInit"]);
    EXPECT_EQ(2U, output["compositeEffecterCount"]);
    EXPECT_EQ(1, output["possibleStatesSize[1]"]);
    EXPECT_EQ(2, output["possibleStatesSize[0]"]);
}

TEST(PldmtoolPlatformPdrTest, TruncatedStateEffecterFixedPartIsRejected)
{
    auto record = makeStateEffecterRecord(0, 0, {});
    record.pop_back();

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("effecterID"));
}

TEST(PldmtoolPlatformPdrTest, StateEffecterInitOutsideTheNameTableIsRejected)
{
    // effecter_init names one of four states; 4 indexes past the table.
    auto record = makeStateEffecterRecord(4, 1, makePossibleStates(1, 1, 1));

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("effecterInit"));
    EXPECT_FALSE(output.contains("stateSetID[0]"));
}

TEST(PldmtoolPlatformPdrTest,
     StateEffecterPossibleStatesSizeBeyondRecordIsRejected)
{
    // possible_states_size claims 200 bit-field bytes, one is present.
    auto record = makeStateEffecterRecord(0, 1, makePossibleStates(1, 200, 1));

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("stateSetID[0]"));
    EXPECT_FALSE(output.contains("possibleStates[0]"));
}

TEST(PldmtoolPlatformPdrTest, StateEffecterCompositeCountBeyondRecordIsRejected)
{
    // composite_effecter_count claims 3 entries, one is present.
    auto record = makeStateEffecterRecord(0, 3, makePossibleStates(1, 1, 1));

    auto output = runGetPdrParseResponse(record);

    EXPECT_TRUE(output.contains("stateSetID[2]"));
    EXPECT_FALSE(output.contains("stateSetID[1]"));
    EXPECT_FALSE(output.contains("stateSetID[0]"));
}

std::vector<uint8_t> makeAuxNameRecord(
    uint8_t type, const std::vector<uint8_t>& names, uint8_t count = 1)
{
    std::vector<uint8_t> body;
    put16(body, 1); // terminus_handle
    put16(body, 7); // sensor_id / effecter_id
    body.push_back(count);
    body.insert(body.end(), names.begin(), names.end());
    return makeRecord(type, body);
}

// nameStringCount=1, language tag "eng", UTF-16BE name "A". The UTF-16 string
// starts at an even offset of the record.
const std::vector<uint8_t> validAuxNames = {1,    'e',  'n',  'g', 0,
                                            0x00, 0x41, 0x00, 0x00};

TEST(PldmtoolPlatformPdrTest, ValidEffecterAuxNamePdrIsFormatted)
{
    auto record =
        makeAuxNameRecord(PLDM_EFFECTER_AUXILIARY_NAMES_PDR, validAuxNames);

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(7, output["effecterId"]);
    EXPECT_EQ("eng", output["effecter0_nameLanguageTag0"]);
    EXPECT_EQ("A", output["effecter0_entityAuxName0"]);
}

TEST(PldmtoolPlatformPdrTest, ValidSensorAuxNamePdrIsFormatted)
{
    auto record =
        makeAuxNameRecord(PLDM_SENSOR_AUXILIARY_NAMES_PDR, validAuxNames);

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(7, output["sensorId"]);
    EXPECT_EQ("eng", output["sensor0_nameLanguageTag0"]);
    EXPECT_EQ("A", output["sensor0_entityAuxName0"]);
}

TEST(PldmtoolPlatformPdrTest, AuxNameUtf16StartingAtOddOffsetIsFormatted)
{
    // "en" + NUL is three bytes, so the UTF-16 string starts at an odd offset.
    const std::vector<uint8_t> names = {1, 'e', 'n', 0, 0x00, 0x41, 0x00, 0x00};
    auto record = makeAuxNameRecord(PLDM_EFFECTER_AUXILIARY_NAMES_PDR, names);

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ("en", output["effecter0_nameLanguageTag0"]);
    EXPECT_EQ("A", output["effecter0_entityAuxName0"]);
}

TEST(PldmtoolPlatformPdrTest, AuxNameCountWithoutAnyNameBytesIsRejected)
{
    auto record = makeAuxNameRecord(PLDM_EFFECTER_AUXILIARY_NAMES_PDR, {});

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("effecter0_nameLanguageTag0"));
}

TEST(PldmtoolPlatformPdrTest, AuxNameLanguageTagWithoutTerminatorIsRejected)
{
    const std::vector<uint8_t> names = {1, 'e', 'n', 'g', 'l', 'i', 's', 'h'};
    auto record = makeAuxNameRecord(PLDM_EFFECTER_AUXILIARY_NAMES_PDR, names);

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("effecter0_nameLanguageTag0"));
}

TEST(PldmtoolPlatformPdrTest, AuxNameUtf16StringWithoutTerminatorIsRejected)
{
    const std::vector<uint8_t> names = {1,    'e',  'n',  'g',  0,    0x00,
                                        0x41, 0x00, 0x42, 0x00, 0x43, 0x00};
    auto record = makeAuxNameRecord(PLDM_EFFECTER_AUXILIARY_NAMES_PDR, names);

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("effecter0_nameLanguageTag0"));
    EXPECT_FALSE(output.contains("effecter0_entityAuxName0"));
}

TEST(PldmtoolPlatformPdrTest, AuxNameStringCountBeyondRecordIsRejected)
{
    // nameStringCount claims 255 names, one is present.
    std::vector<uint8_t> names = validAuxNames;
    names[0] = 255;
    auto record = makeAuxNameRecord(PLDM_EFFECTER_AUXILIARY_NAMES_PDR, names);

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ("eng", output["effecter0_nameLanguageTag0"]);
    EXPECT_FALSE(output.contains("effecter1_nameLanguageTag0"));
}

// The DSP0248 limit of 256 counts the terminator, so the longest accepted
// string has 255 characters. Terminus::parseSensorAuxiliaryNamesPDR() rejects
// longer names the same way.
std::vector<uint8_t> makeAuxNamesOfLength(size_t tagChars, size_t nameUnits)
{
    std::vector<uint8_t> names = {1};
    names.insert(names.end(), tagChars, 'e');
    names.push_back(0);
    for (size_t i = 0; i < nameUnits; ++i)
    {
        names.push_back(0x00);
        names.push_back(0x41);
    }
    names.push_back(0);
    names.push_back(0);
    return names;
}

TEST(PldmtoolPlatformPdrTest, AuxNameStringsAtTheLengthLimitAreFormatted)
{
    auto record = makeAuxNameRecord(PLDM_EFFECTER_AUXILIARY_NAMES_PDR,
                                    makeAuxNamesOfLength(255, 255));

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(std::string(255, 'e'),
              output["effecter0_nameLanguageTag0"].get<std::string>());
    EXPECT_EQ(std::string(255, 'A'),
              output["effecter0_entityAuxName0"].get<std::string>());
}

TEST(PldmtoolPlatformPdrTest, AuxNameLanguageTagOverTheLengthLimitIsRejected)
{
    auto record = makeAuxNameRecord(PLDM_EFFECTER_AUXILIARY_NAMES_PDR,
                                    makeAuxNamesOfLength(256, 1));

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("effecter0_nameLanguageTag0"));
}

TEST(PldmtoolPlatformPdrTest, AuxNameUtf16NameOverTheLengthLimitIsRejected)
{
    auto record = makeAuxNameRecord(PLDM_EFFECTER_AUXILIARY_NAMES_PDR,
                                    makeAuxNamesOfLength(3, 256));

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("effecter0_nameLanguageTag0"));
    EXPECT_FALSE(output.contains("effecter0_entityAuxName0"));
}

std::vector<uint8_t> makeTerminusLocatorRecord(
    uint8_t locatorType, uint8_t valueSize, const std::vector<uint8_t>& value)
{
    std::vector<uint8_t> body;
    put16(body, 1);    // terminus_handle
    body.push_back(1); // validity
    body.push_back(9); // tid
    put16(body, 0);    // container_id
    body.push_back(locatorType);
    body.push_back(valueSize);
    body.insert(body.end(), value.begin(), value.end());
    return makeRecord(PLDM_TERMINUS_LOCATOR_PDR, body);
}

TEST(PldmtoolPlatformPdrTest, ValidMctpTerminusLocatorPdrIsFormatted)
{
    auto record = makeTerminusLocatorRecord(PLDM_TERMINUS_LOCATOR_TYPE_MCTP_EID,
                                            1, {0x2a});

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ("MCTP_EID", output["terminusLocatorType"]);
    EXPECT_EQ(0x2aU, output["EID"]);
}

TEST(PldmtoolPlatformPdrTest, TerminusLocatorTypeOutsideTheNameTableIsRejected)
{
    // Four locator types are named; 4 indexes past the table.
    auto record = makeTerminusLocatorRecord(4, 1, {0x2a});

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("terminusLocatorType"));
}

TEST(PldmtoolPlatformPdrTest, MctpTerminusLocatorWithoutEidByteIsRejected)
{
    auto record =
        makeTerminusLocatorRecord(PLDM_TERMINUS_LOCATOR_TYPE_MCTP_EID, 1, {});

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("EID"));
}

TEST(PldmtoolPlatformPdrTest, TruncatedTerminusLocatorFixedPartIsRejected)
{
    auto record =
        makeTerminusLocatorRecord(PLDM_TERMINUS_LOCATOR_TYPE_UID, 0, {});
    record.pop_back();

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("PLDMTerminusHandle"));
}

std::vector<uint8_t> makeFruRecordSetRecord(size_t bodyBytes)
{
    std::vector<uint8_t> body;
    put16(body, 1); // terminus_handle
    put16(body, 5); // fru_rsi
    put16(body, PLDM_ENTITY_SYS_BOARD);
    put16(body, 1); // entity_instance
    put16(body, 0); // container_id
    body.resize(bodyBytes);
    return makeRecord(PLDM_PDR_FRU_RECORD_SET, body);
}

TEST(PldmtoolPlatformPdrTest, ValidFruRecordSetPdrIsFormatted)
{
    auto record = makeFruRecordSetRecord(sizeof(pldm_pdr_fru_record_set));

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(1U, output["PLDMTerminusHandle"]);
    EXPECT_EQ(5U, output["FRURecordSetIdentifier"]);
}

TEST(PldmtoolPlatformPdrTest, TruncatedFruRecordSetIsRejected)
{
    auto record = makeFruRecordSetRecord(sizeof(pldm_pdr_fru_record_set) - 1);

    auto output = runGetPdrParseResponse(record);

    EXPECT_FALSE(output.contains("PLDMTerminusHandle"));
}

TEST(PldmtoolPlatformPdrTest, RecordShorterThanThePdrHeaderIsRejected)
{
    std::vector<uint8_t> record(sizeof(pldm_pdr_hdr) - 1, 0);

    auto output = runGetPdrParseResponse(record);

    EXPECT_EQ(record.size(), output["responseCount"]);
    EXPECT_FALSE(output.contains("PDRType"));
}

} // namespace
