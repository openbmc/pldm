#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "libpldmresponder/platform.hpp"
#include "libpldmresponder/platform_state_effecter.hpp"
#include "mocked_utils.hpp"
#include "utils.hpp"

#include <iostream>

using namespace pldm::utils;
using namespace pldm::responder;
using namespace pldm::responder::platform;
using namespace pldm::responder::pdr;
using namespace pldm::responder::pdr_utils;

TEST(getPDR, testGoodPath)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto req = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    struct pldm_get_pdr_req* request =
        reinterpret_cast<struct pldm_get_pdr_req*>(req->payload);
    request->request_count = 100;

    Handler handler("./pdr_jsons/state_effecter/good");
    Repo& repo = handler.getRepo();

    ASSERT_EQ(repo.empty(), false);
    auto response = handler.getPDR(req, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    struct pldm_get_pdr_resp* resp =
        reinterpret_cast<struct pldm_get_pdr_resp*>(responsePtr->payload);
    ASSERT_EQ(PLDM_SUCCESS, resp->completion_code);
    ASSERT_EQ(2, resp->next_record_handle);
    ASSERT_EQ(true, resp->response_count != 0);

    pldm_pdr_hdr* hdr = reinterpret_cast<pldm_pdr_hdr*>(resp->record_data);
    ASSERT_EQ(hdr->record_handle, 1);
    ASSERT_EQ(hdr->version, 1);
}

TEST(getPDR, testShortRead)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto req = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    struct pldm_get_pdr_req* request =
        reinterpret_cast<struct pldm_get_pdr_req*>(req->payload);
    request->request_count = 1;

    Handler handler("./pdr_jsons/state_effecter/good");
    Repo& repo = handler.getRepo();

    ASSERT_EQ(repo.empty(), false);
    auto response = handler.getPDR(req, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_pdr_resp* resp =
        reinterpret_cast<struct pldm_get_pdr_resp*>(responsePtr->payload);
    ASSERT_EQ(PLDM_SUCCESS, resp->completion_code);
    ASSERT_EQ(1, resp->response_count);
}

TEST(getPDR, testBadRecordHandle)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto req = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    struct pldm_get_pdr_req* request =
        reinterpret_cast<struct pldm_get_pdr_req*>(req->payload);
    request->record_handle = 100000;
    request->request_count = 1;

    Handler handler("./pdr_jsons/state_effecter/good");
    Repo& repo = handler.getRepo();

    ASSERT_EQ(repo.empty(), false);
    auto response = handler.getPDR(req, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    ASSERT_EQ(responsePtr->payload[0], PLDM_PLATFORM_INVALID_RECORD_HANDLE);
}

TEST(getPDR, testNoNextRecord)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto req = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    struct pldm_get_pdr_req* request =
        reinterpret_cast<struct pldm_get_pdr_req*>(req->payload);
    request->record_handle = 1;

    Handler handler("./pdr_jsons/state_effecter/good");
    Repo& repo = handler.getRepo();

    ASSERT_EQ(repo.empty(), false);
    auto response = handler.getPDR(req, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_pdr_resp* resp =
        reinterpret_cast<struct pldm_get_pdr_resp*>(responsePtr->payload);
    ASSERT_EQ(PLDM_SUCCESS, resp->completion_code);
    ASSERT_EQ(2, resp->next_record_handle);
}

TEST(getPDR, testFindPDR)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto req = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    struct pldm_get_pdr_req* request =
        reinterpret_cast<struct pldm_get_pdr_req*>(req->payload);
    request->request_count = 100;

    Handler handler("./pdr_jsons/state_effecter/good");
    Repo& repo = handler.getRepo();

    ASSERT_EQ(repo.empty(), false);
    auto response = handler.getPDR(req, requestPayloadLength);

    // Let's try to find a PDR of type stateEffecter (= 11) and entity type =
    // 100
    bool found = false;
    uint32_t handle = 0; // start asking for PDRs from recordHandle 0
    while (!found)
    {
        request->record_handle = handle;
        auto response = handler.getPDR(req, requestPayloadLength);
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        struct pldm_get_pdr_resp* resp =
            reinterpret_cast<struct pldm_get_pdr_resp*>(responsePtr->payload);
        ASSERT_EQ(PLDM_SUCCESS, resp->completion_code);

        handle = resp->next_record_handle; // point to the next pdr in case
                                           // current is not what we want

        pldm_pdr_hdr* hdr = reinterpret_cast<pldm_pdr_hdr*>(resp->record_data);
        std::cerr << "PDR next record handle " << handle << "\n";
        std::cerr << "PDR type " << hdr->type << "\n";
        if (hdr->type == PLDM_STATE_EFFECTER_PDR)
        {
            pldm_state_effecter_pdr* pdr =
                reinterpret_cast<pldm_state_effecter_pdr*>(resp->record_data);
            std::cerr << "PDR entity type " << pdr->entity_type << "\n";
            if (pdr->entity_type == 100)
            {
                found = true;
                // Rest of the PDR can be accessed as need be
                break;
            }
        }
        if (!resp->next_record_handle) // no more records
        {
            break;
        }
    }
    ASSERT_EQ(found, true);
}

TEST(setStateEffecterStatesHandler, testGoodRequest)
{
    Handler handler("./pdr_jsons/state_effecter/good");
    pdr_utils::Repo outRepo =
        getRepoByType(handler.getRepo(), PLDM_STATE_EFFECTER_PDR);

    pdr_utils::PdrEntry e;
    auto record1 = pdr::getRecordByHandle(outRepo, 1, e);
    ASSERT_NE(record1, nullptr);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(e.data);
    EXPECT_EQ(pdr->hdr.type, PLDM_STATE_EFFECTER_PDR);

    std::vector<set_effecter_state_field> stateField;
    stateField.push_back({PLDM_REQUEST_SET, 1});
    stateField.push_back({PLDM_REQUEST_SET, 1});
    std::string value = "xyz.openbmc_project.Foo.Bar.V1";
    PropertyValue propertyValue = value;

    MockdBusHandler handlerObj;
    DBusMapping dbusMapping{"/foo/bar", "xyz.openbmc_project.Foo.Bar",
                            "propertyName", "string"};

    EXPECT_CALL(handlerObj, setDbusProperty(dbusMapping, propertyValue))
        .Times(2);
    auto rc = platform_state_effecter::setStateEffecterStatesHandler<
        MockdBusHandler, Handler>(handlerObj, handler, 0x1, stateField);
    ASSERT_EQ(rc, 0);
}

TEST(setStateEffecterStatesHandler, testBadRequest)
{
    Handler handler("./pdr_jsons/state_effecter/good");
    pdr_utils::Repo outRepo =
        getRepoByType(handler.getRepo(), PLDM_STATE_EFFECTER_PDR);

    pdr_utils::PdrEntry e;
    auto record1 = pdr::getRecordByHandle(outRepo, 1, e);
    ASSERT_NE(record1, nullptr);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(e.data);
    EXPECT_EQ(pdr->hdr.type, PLDM_STATE_EFFECTER_PDR);

    std::vector<set_effecter_state_field> stateField;
    stateField.push_back({PLDM_REQUEST_SET, 3});
    stateField.push_back({PLDM_REQUEST_SET, 4});

    MockdBusHandler handlerObj;
    auto rc = platform_state_effecter::setStateEffecterStatesHandler<
        MockdBusHandler, Handler>(handlerObj, handler, 0x1, stateField);
    ASSERT_EQ(rc, PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE);

    rc = platform_state_effecter::setStateEffecterStatesHandler<MockdBusHandler,
                                                                Handler>(
        handlerObj, handler, 0x9, stateField);
    ASSERT_EQ(rc, PLDM_PLATFORM_INVALID_EFFECTER_ID);

    stateField.push_back({PLDM_REQUEST_SET, 4});
    rc = platform_state_effecter::setStateEffecterStatesHandler<MockdBusHandler,
                                                                Handler>(
        handlerObj, handler, 0x1, stateField);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}
