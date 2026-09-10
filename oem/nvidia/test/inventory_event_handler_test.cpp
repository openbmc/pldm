#include "oem/nvidia/inventory_event_handler.hpp"

#include <libpldm/base.h>

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

using namespace pldm::oem_nvidia;

namespace
{

constexpr pldm_tid_t testTid = 1;
constexpr std::string_view document = R"({"Processor":[]})";

std::vector<uint8_t> makeEvent(std::string_view payload, uint16_t declaredSize)
{
    std::vector<uint8_t> event{0x01, 0x00,
                               static_cast<uint8_t>(declaredSize & 0xFF),
                               static_cast<uint8_t>(declaredSize >> 8)};
    event.insert(event.end(), payload.begin(), payload.end());
    return event;
}

std::vector<uint8_t> makeEvent(std::string_view payload)
{
    return makeEvent(payload, static_cast<uint16_t>(payload.size()));
}

} // namespace

TEST(ParseInventoryEventPayload, ReturnsDocumentAfterHeader)
{
    auto event = makeEvent(document);

    auto payload = parseInventoryEventPayload(event.data(), event.size());

    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(document, *payload);
}

TEST(ParseInventoryEventPayload, HonoursDeclaredSizeOverTrailingBytes)
{
    auto event = makeEvent(document, 5);

    auto payload = parseInventoryEventPayload(event.data(), event.size());

    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(document.substr(0, 5), *payload);
}

TEST(ParseInventoryEventPayload, TrimsTerminatorCountedInDeclaredSize)
{
    std::string terminated(document);
    terminated.push_back('\0');
    auto event = makeEvent(terminated);

    auto payload = parseInventoryEventPayload(event.data(), event.size());

    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(document, *payload);
}

TEST(ParseInventoryEventPayload, RejectsPayloadOfOnlyTerminators)
{
    auto event = makeEvent(std::string_view("\0\0", 2));

    EXPECT_FALSE(
        parseInventoryEventPayload(event.data(), event.size()).has_value());
}

TEST(ParseInventoryEventPayload, RejectsEventShorterThanHeader)
{
    std::vector<uint8_t> event{0x01, 0x00, 0x04};

    EXPECT_FALSE(
        parseInventoryEventPayload(event.data(), event.size()).has_value());
}

TEST(ParseInventoryEventPayload, RejectsTruncatedPayload)
{
    auto event = makeEvent(document);
    event.resize(event.size() - 1);

    EXPECT_FALSE(
        parseInventoryEventPayload(event.data(), event.size()).has_value());
}

TEST(ParseInventoryEventPayload, RejectsEmptyPayload)
{
    auto event = makeEvent({}, 0);

    EXPECT_FALSE(
        parseInventoryEventPayload(event.data(), event.size()).has_value());
}

TEST(ParseInventoryEventPayload, RejectsNullEventData)
{
    EXPECT_FALSE(parseInventoryEventPayload(nullptr, 16).has_value());
}

TEST(ProcessorModuleIndex, ParsesModuleSuffix)
{
    EXPECT_EQ(0, processorModuleIndex("ProcessorModule_0"));
    EXPECT_EQ(1, processorModuleIndex("ProcessorModule_1"));
}

TEST(ProcessorModuleIndex, RejectsOtherTerminusNames)
{
    EXPECT_FALSE(processorModuleIndex("Terminus_1").has_value());
    EXPECT_FALSE(processorModuleIndex("").has_value());
}

TEST(ProcessorModuleIndex, RejectsMissingOrTrailingSuffix)
{
    EXPECT_FALSE(processorModuleIndex("ProcessorModule_").has_value());
    EXPECT_FALSE(processorModuleIndex("ProcessorModule_0a").has_value());
}

TEST(HandleInventoryEvent, RejectsMalformedEvent)
{
    std::vector<uint8_t> event{0x01, 0x00};

    EXPECT_EQ(PLDM_ERROR_INVALID_DATA,
              handleInventoryEvent(testTid, "ProcessorModule_0", event.data(),
                                   event.size()));
}

TEST(HandleInventoryEvent, RejectsTerminusWithoutModuleIndex)
{
    auto event = makeEvent(document);

    EXPECT_EQ(PLDM_ERROR_INVALID_DATA,
              handleInventoryEvent(testTid, "Terminus_1", event.data(),
                                   event.size()));
}
