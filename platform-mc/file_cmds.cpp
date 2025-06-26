#include "file_cmds.hpp"

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{
namespace file_cmds
{

exec::task<int> dfOpen(TerminusManager& terminusManager, const pldm_tid_t& tid,
                       const FileID& fileId, const bitfield16_t& openAttr,
                       FD& fd)
{
    const struct pldm_file_df_open_req req_data = {fileId, openAttr};

    Request request(sizeof(pldm_msg_hdr) + PLDM_DF_OPEN_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;
    auto rc = encode_pldm_file_df_open_req(0, &req_data, requestMsg,
                                           PLDM_DF_OPEN_REQ_BYTES);

    if (rc != PLDM_SUCCESS)
    {
        lg2::error(
            "Failed to encode DfOpen request for terminus ID {TID}, FileIdentifier {ID}, error {RC} ",
            "TID", tid, "ID", fileId, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send DfOpen request for terminus ID {TID}, FileIdentifier {ID}, error {RC} ",
            "TID", tid, "ID", fileId, "RC", rc);
        co_return rc;
    }

    struct pldm_file_df_open_resp resp_data = {};
    rc = decode_pldm_file_df_open_resp(responseMsg, responseLen, &resp_data);

    if (rc)
    {
        lg2::error(
            "Failed to decode DfOpen response for terminus ID {TID}, FileIdentifier {ID}, error {RC} ",
            "TID", tid, "ID", fileId, "RC", rc);
        co_return rc;
    }

    if (resp_data.completion_code)
    {
        lg2::error(
            "Failed to send DfOpen request for terminus ID {TID}, FileIdentifier {ID}, completion code '{CC}'",
            "TID", tid, "ID", fileId, "CC", resp_data.completion_code);
        co_return resp_data.completion_code;
    }

    fd = resp_data.file_descriptor;

    co_return rc;
}

exec::task<int> dfClose(TerminusManager& terminusManager, const pldm_tid_t& tid,
                        const FD& fd, const bitfield16_t& closeOptions)
{
    Request request(sizeof(pldm_msg_hdr) + PLDM_DF_CLOSE_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;

    const struct pldm_file_df_close_req req_data = {fd, closeOptions};

    auto rc = encode_pldm_file_df_close_req(0, &req_data, requestMsg,
                                            PLDM_DF_CLOSE_REQ_BYTES);
    if (rc != PLDM_SUCCESS)
    {
        lg2::error(
            "Failed to encode DfClose request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "FD", fd, "FD", fd, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send DfClose request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", fd, "RC", rc);
        co_return rc;
    }

    struct pldm_file_df_close_resp resp_data = {};
    rc = decode_pldm_file_df_close_resp(responseMsg, responseLen, &resp_data);
    if (rc)
    {
        lg2::error(
            "Failed to decode DfClose response for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", fd, "RC", rc);
        co_return rc;
    }

    if (resp_data.completion_code)
    {
        lg2::error(
            "Failed to send DfClose request for terminus ID {TID}, file descriptor {FD}, completion code '{CC}'",
            "TID", tid, "FD", fd, "CC", resp_data.completion_code);
        co_return resp_data.completion_code;
    }

    co_return rc;
}

exec::task<int> dfHeartbeat(
    TerminusManager& terminusManager, const pldm_tid_t& tid, const FD& fd,
    const Interval& reqMaxInterval, Interval& respMaxInterval)
{
    Request request(sizeof(pldm_msg_hdr) + PLDM_DF_HEARTBEAT_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;

    const struct pldm_file_df_heartbeat_req req_data = {fd, reqMaxInterval};

    auto rc = encode_pldm_file_df_heartbeat_req(0, &req_data, requestMsg,
                                                PLDM_DF_HEARTBEAT_REQ_BYTES);
    if (rc != PLDM_SUCCESS)
    {
        lg2::error(
            "Failed to encode DfHeartbeat request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", fd, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send DfHeartbeat request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", fd, "RC", rc);
        co_return rc;
    }

    struct pldm_file_df_heartbeat_resp resp_data = {};
    rc = decode_pldm_file_df_heartbeat_resp(responseMsg, responseLen,
                                            &resp_data);
    if (rc)
    {
        lg2::error(
            "Failed to decode DfHeartbeat response for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", fd, "RC", rc);
        co_return rc;
    }

    if (resp_data.completion_code)
    {
        lg2::error(
            "Failed to send DfHeartbeat request for terminus ID {TID}, file descriptor {FD}, completion code '{CC}'",
            "TID", tid, "FD", fd, "CC", resp_data.completion_code);
        co_return resp_data.completion_code;
    }

    respMaxInterval = resp_data.responder_max_interval;

    co_return rc;
}

exec::task<int> negotiateTransferParameters(
    TerminusManager& terminusManager, const pldm_tid_t& tid,
    const PartSize& reqPartSize, const bitfield8_t* const reqProtocol,
    PartSize& respPartSize, bitfield8_t* const respProtocol)
{
    Request request(sizeof(pldm_msg_hdr) +
                    PLDM_BASE_NEGOTIATE_TRANSFER_PARAMETERS_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;

    struct pldm_base_negotiate_transfer_params_req req_data_prep;

    req_data_prep.requester_part_size = reqPartSize;
    memcpy(req_data_prep.requester_protocol_support, reqProtocol,
           8 * sizeof(*reqProtocol));

    const struct pldm_base_negotiate_transfer_params_req req_data =
        req_data_prep;

    auto rc = encode_pldm_base_negotiate_transfer_params_req(
        0, &req_data, requestMsg,
        PLDM_BASE_NEGOTIATE_TRANSFER_PARAMETERS_REQ_BYTES);
    if (rc != PLDM_SUCCESS)
    {
        lg2::error(
            "Failed to encode NegotiateTransferParameters request for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
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

    struct pldm_base_negotiate_transfer_params_resp resp_data = {};
    rc = decode_pldm_base_negotiate_transfer_params_resp(
        responseMsg, responseLen, &resp_data);
    if (rc)
    {
        lg2::error(
            "Failed to decode NegotiateTransferParameters response for terminus ID {TID}, error {RC} ",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (resp_data.completion_code)
    {
        lg2::error(
            "Failed to send NegotiateTransferParameters request for terminus ID {TID}, completion code '{CC}'",
            "TID", tid, "CC", resp_data.completion_code);
        co_return resp_data.completion_code;
    }

    respPartSize = resp_data.responder_part_size;
    memcpy(respProtocol, resp_data.responder_protocol_support,
           8 * sizeof(*respProtocol));
    co_return rc;
}

exec::task<int> sendMultipartReceive(
    TerminusManager& terminusManager, const pldm_tid_t& tid,
    const TransferOp& transferOp, const TransferCtx& fd,
    const TransferHandle& transferHandle,
    const SectionOffset& requestSectionOffset,
    const SectionOffset& requestSectionLength, uint8_t& completionCode,
    TransferFlag& transferFlag, TransferHandle& nextDataTransferHandle,
    std::vector<uint8_t>& dataBuffer, Checksum& dataIntegrityChecksum)
{
    Request request(sizeof(pldm_msg_hdr) + PLDM_MULTIPART_RECEIVE_REQ_BYTES);
    auto requestMsg = new (request.data()) pldm_msg;

    lg2::debug(
        "MultipartReceive request: transferOperation={TRANSOP} transferContext={TRANSCTX} transferHandle={TRANSHANDLE} requestSectionOffset={REQOFFSET} requestSectionLength={REQLEN}",
        "TRANSOP", transferOp, "TRANSCTX", fd, "TRANSHANDLE", transferHandle,
        "REQOFFSET", requestSectionOffset, "REQLEN", requestSectionLength);

    const struct pldm_multipart_receive_req req_data = {
        PLDM_BASE,      transferOp,           fd,
        transferHandle, requestSectionOffset, requestSectionLength};
    auto rc = encode_pldm_base_multipart_receive_req(
        0, &req_data, requestMsg, PLDM_MULTIPART_RECEIVE_REQ_BYTES);
    if (rc != PLDM_SUCCESS)
    {
        lg2::error(
            "Failed to encode MultipartReceive request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", fd, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = nullptr;
    size_t responseLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &responseLen);
    if (rc)
    {
        lg2::error(
            "Failed to send MultipartReceive request for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", fd, "RC", rc);
        co_return rc;
    }

    struct pldm_base_multipart_receive_resp resp_data = {};

    rc = decode_pldm_base_multipart_receive_resp(responseMsg, responseLen,
                                            &resp_data, &dataIntegrityChecksum);
    if (rc)
    {
        lg2::error(
            "Failed to decode MultipartReceive response for terminus ID {TID}, file descriptor {FD}, error {RC} ",
            "TID", tid, "FD", fd, "RC", rc);
        co_return rc;
    }

    completionCode = resp_data.completion_code;
    transferFlag = resp_data.transfer_flag;
    nextDataTransferHandle = resp_data.next_transfer_handle;

    dataBuffer.insert(dataBuffer.end(), resp_data.data.ptr,
                      resp_data.data.ptr + resp_data.data.length);

    lg2::debug(
        "MultipartReceive response: transferFlag={TRANSFLAG} nextDataTransferHandle={TRANSHANDLE} dataSize={DATASIZE} dataIntegrityChecksum={CRC}",
        "TRANSFLAG", transferFlag, "TRANSHANDLE", nextDataTransferHandle,
        "DATASIZE", resp_data.data.length, "CRC", dataIntegrityChecksum);

    if (completionCode)
    {
        lg2::error(
            "Failed to send MultipartReceive request for terminus ID {TID}, file descriptor {FD}, completion code '{CC}'",
            "TID", tid, "FD", fd, "CC", resp_data.completion_code);
        co_return completionCode;
    }

    co_return rc;
}

} // namespace file_cmds

} // namespace platform_mc

} // namespace pldm
