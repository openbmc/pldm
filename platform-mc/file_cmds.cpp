#include "file_cmds.hpp"

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{
namespace file_cmds
{

exec::task<int> dfOpen(TerminusManager& terminusManager, pldm_tid_t tid,
                       const struct pldm_file_df_open_req* reqPtr,
                       struct pldm_file_df_open_resp* respPtr)
{
    size_t payload_length = PLDM_DF_OPEN_REQ_BYTES;
    Request request(sizeof(pldm_msg_hdr) + payload_length);
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc =
        encode_pldm_file_df_open_req(0, reqPtr, requestMsg, &payload_length);

    if (rc)
    {
        lg2::error(
            "Failed to encode DfOpen request for terminus ID {TID}, FileIdentifier {ID}, error {RC} ",
            "TID", tid, "ID", reqPtr->file_identifier, "RC", rc);
        co_return PLDM_ERROR;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send DfOpen request for terminus ID {TID}, FileIdentifier {ID}, error {RC} ",
            "TID", tid, "ID", reqPtr->file_identifier, "RC", rc);
        co_return rc;
    }

    rc = decode_pldm_file_df_open_resp(responseMsg, responseLen, respPtr);

    if (rc)
    {
        lg2::error(
            "Failed to decode DfOpen response for terminus ID {TID}, FileIdentifier {ID}, error {RC} ",
            "TID", tid, "ID", reqPtr->file_identifier, "RC", rc);
        co_return PLDM_ERROR;
    }

    if (respPtr->completion_code)
    {
        lg2::error(
            "Failed to send DfOpen request for terminus ID {TID}, FileIdentifier {ID}, completion code {CC}",
            "TID", tid, "ID", reqPtr->file_identifier, "CC",
            respPtr->completion_code);
        co_return respPtr->completion_code;
    }

    co_return respPtr->completion_code;
}

exec::task<int> dfClose(TerminusManager& terminusManager, pldm_tid_t tid,
                        const struct pldm_file_df_close_req* reqPtr)
{
    size_t payload_length = PLDM_DF_CLOSE_REQ_BYTES;
    Request request(sizeof(pldm_msg_hdr) + payload_length);
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc =
        encode_pldm_file_df_close_req(0, reqPtr, requestMsg, &payload_length);
    if (rc)
    {
        lg2::error(
            "Failed to encode DfClose request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "FD", reqPtr->file_descriptor, "RC", rc);
        co_return PLDM_ERROR;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send DfClose request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", reqPtr->file_descriptor, "RC", rc);
        co_return rc;
    }

    struct pldm_file_df_close_resp resp_data = {};
    rc = decode_pldm_file_df_close_resp(responseMsg, responseLen, &resp_data);
    if (rc)
    {
        lg2::error(
            "Failed to decode DfClose response for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", reqPtr->file_descriptor, "RC", rc);
        co_return PLDM_ERROR;
    }

    if (resp_data.completion_code)
    {
        lg2::error(
            "Failed to send DfClose request for terminus ID {TID}, file descriptor {FD}, completion code {CC}",
            "TID", tid, "FD", reqPtr->file_descriptor, "CC",
            resp_data.completion_code);
        co_return resp_data.completion_code;
    }

    co_return resp_data.completion_code;
}

exec::task<int> dfHeartbeat(TerminusManager& terminusManager, pldm_tid_t tid,
                            const struct pldm_file_df_heartbeat_req* reqPtr,
                            struct pldm_file_df_heartbeat_resp* respPtr)
{
    size_t payload_length = PLDM_DF_HEARTBEAT_REQ_BYTES;
    Request request(sizeof(pldm_msg_hdr) + payload_length);
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc = encode_pldm_file_df_heartbeat_req(0, reqPtr, requestMsg,
                                                &payload_length);
    if (rc)
    {
        lg2::error(
            "Failed to encode DfHeartbeat request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", reqPtr->file_descriptor, "RC", rc);
        co_return PLDM_ERROR;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send DfHeartbeat request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", reqPtr->file_descriptor, "RC", rc);
        co_return rc;
    }

    rc = decode_pldm_file_df_heartbeat_resp(responseMsg, responseLen, respPtr);
    if (rc)
    {
        lg2::error(
            "Failed to decode DfHeartbeat response for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", reqPtr->file_descriptor, "RC", rc);
        co_return PLDM_ERROR;
    }

    if (respPtr->completion_code)
    {
        lg2::error(
            "Failed to send DfHeartbeat request for terminus ID {TID}, file descriptor {FD}, completion code {CC}",
            "TID", tid, "FD", reqPtr->file_descriptor, "CC",
            respPtr->completion_code);
        co_return respPtr->completion_code;
    }

    co_return respPtr->completion_code;
}

exec::task<int> negotiateTransferParameters(
    TerminusManager& terminusManager, pldm_tid_t tid,
    const struct pldm_base_negotiate_transfer_params_req* reqPtr,
    struct pldm_base_negotiate_transfer_params_resp* respPtr)
{
    size_t payload_length = PLDM_BASE_NEGOTIATE_TRANSFER_PARAMETERS_REQ_BYTES;
    Request request(sizeof(pldm_msg_hdr) + payload_length);
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc = encode_pldm_base_negotiate_transfer_params_req(
        0, reqPtr, requestMsg, &payload_length);
    if (rc)
    {
        lg2::error(
            "Failed to encode NegotiateTransferParameters request for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return PLDM_ERROR;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send NegotiateTransferParameters request for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    rc = decode_pldm_base_negotiate_transfer_params_resp(responseMsg,
                                                         responseLen, respPtr);
    if (rc)
    {
        lg2::error(
            "Failed to decode NegotiateTransferParameters response for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return PLDM_ERROR;
    }

    if (respPtr->completion_code)
    {
        lg2::error(
            "Failed to send NegotiateTransferParameters request for terminus ID {TID}, completion code {CC}",
            "TID", tid, "CC", respPtr->completion_code);
        co_return respPtr->completion_code;
    }

    co_return respPtr->completion_code;
}

exec::task<int> sendMultipartReceive(
    TerminusManager& terminusManager, pldm_tid_t tid,
    const struct pldm_base_multipart_receive_req* reqPtr,
    struct pldm_base_multipart_receive_resp* respPtr,
    CRC32Type& dataIntegrityChecksum)
{
    size_t payload_length = PLDM_MULTIPART_RECEIVE_REQ_BYTES;
    Request request(sizeof(pldm_msg_hdr) + payload_length);
    auto requestMsg = new (request.data()) pldm_msg;

    auto rc = encode_pldm_base_multipart_receive_req(0, reqPtr, requestMsg,
                                                     &payload_length);
    if (rc)
    {
        lg2::error(
            "Failed to encode MultipartReceive request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", reqPtr->transfer_ctx, "RC", rc);
        co_return PLDM_ERROR;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send MultipartReceive request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", reqPtr->transfer_ctx, "RC", rc);
        co_return rc;
    }

    rc = decode_pldm_base_multipart_receive_resp(
        responseMsg, responseLen, respPtr, &dataIntegrityChecksum);
    if (rc)
    {
        lg2::error(
            "Failed to decode MultipartReceive response for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", reqPtr->transfer_ctx, "RC", rc);
        co_return PLDM_ERROR;
    }

    if (respPtr->completion_code)
    {
        lg2::error(
            "Failed to send MultipartReceive request for terminus ID {TID}, file descriptor {FD}, completion code {CC}",
            "TID", tid, "FD", reqPtr->transfer_ctx, "CC",
            respPtr->completion_code);
        co_return respPtr->completion_code;
    }

    co_return respPtr->completion_code;
}

} // namespace file_cmds

} // namespace platform_mc

} // namespace pldm
