#include "file_descriptor.hpp"

#include "file_cmds.hpp"

#include <libpldm/edac.h>
#include <libpldm/entity.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstring>
#include <iostream>
#include <string>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{
using namespace file_cmds;
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using NotAllowedMetaData = xyz::openbmc_project::Common::NotAllowed;
using PurposeType =
    sdbusplus::common::xyz::openbmc_project::pldm::File::PurposeType;

// Maximum retry allowed to tolerate local socket back-pressure while the
// consumer drains a large file-transfer stream.
constexpr auto MAX_SOCKET_POLL_RETRY = 120;

// Timeout in milliseconds used in poll() for the server socket.
constexpr auto SOCKET_POLL_TIMEOUT_MS = 50;

// Local socket buffer used between the PLDM transfer task and its consumer.
constexpr int FILE_TRANSFER_SOCKET_BUFFER_SIZE = 1024 * 1024;

// The transfer part size to be negotiated with File Host.
constexpr auto REQUESTER_PART_SIZE = 0x100;

// The interval to send DfHeartbeat to File Host for regular file type.
constexpr auto REQUESTER_MAX_INTERVAL_MS = 500;

// Table 108 – File Descriptor PDR DSP0248 v1.3.0 - FileClassification field.
std::map<FileClassification, PurposeType> filePurposeMap = {
    {FileClassification::OEM, PurposeType::OEM},
    {FileClassification::BootLog, PurposeType::BootLog},
    {FileClassification::SerialTxFIFO, PurposeType::SerialTxFIFO},
    {FileClassification::SerialRxFIFO, PurposeType::SerialRxFIFO},
    {FileClassification::DiagnosticLog, PurposeType::DiagnosticLog},
    {FileClassification::CrashDumpFile, PurposeType::CrashDump},
    {FileClassification::SecurityLog, PurposeType::Security},
    {FileClassification::FRUDataFile, PurposeType::FRU},
    {FileClassification::TelemetryDataFile, PurposeType::TelemetryDataFile},
    {FileClassification::TelemetryDataLog, PurposeType::TelemetryDataLog},
    {FileClassification::FileDirectory, PurposeType::Directory}};

FileDescriptor::FileDescriptor(
    sdbusplus::bus_t& bus, const std::string& path, const pldm_tid_t tid,
    const bool fileDisabled,
    std::shared_ptr<pldm_platform_file_descriptor_pdr> pdr,
    TerminusManager& terminusManager) :
    FileInterface(bus, path.c_str()), tid(tid), terminusManager(terminusManager)
{
    if (!pdr || fileDisabled)
    {
        lg2::error(
            "Terminus ID {TID}: File Descriptor Constructor does not have valid pdr ptr.",
            "TID", tid);
        return;
    }

    entityInfo = std::make_tuple(
        static_cast<EntityType>(pdr->container.entity_type),
        static_cast<EntityInstance>(pdr->container.entity_instance_num),
        static_cast<ContainerID>(pdr->container.entity_container_id));
    isDirectory = ((pdr->container.entity_type & 0x7FFF) ==
                   PLDM_ENTITY_DEVICE_FILE_DIRECTORY);
    isRegular = (pdr->file_capabilities.bits.bit3 == 0);
    exReadPermitted = (pdr->file_capabilities.bits.bit0 == 1);
    identifier = pdr->file_identifier;
    supDirIdentifier = pdr->superior_directory_file_identifier;
    maxSize = pdr->file_maximum_size;
    maxFdCount = pdr->file_maximum_file_descriptor_count;
    fileName.assign(reinterpret_cast<const char*>(pdr->file_name.ptr),
                    pdr->file_name.length);
    name(fileName);
    source(SourceType::ComputerSystem);
    if (pdr->oem_file_classification)
    {
        oemClassName.assign(reinterpret_cast<const char*>(
                                pdr->oem_file_classification_name.ptr),
                            pdr->oem_file_classification_name.length);
    }
    setPurpose(static_cast<FileClassification>(pdr->file_classification));
}

void FileDescriptor::setPurpose(FileClassification classification)
{
    auto it = filePurposeMap.find(classification);
    if (it != filePurposeMap.end())
    {
        purpose(it->second);
    }
    else
    {
        purpose(PurposeType::Unknown);
    }
}

exec::task<int> FileDescriptor::writeDataToSocket(
    size_t dataSize, const uint8_t* const data, int socket)
{
    uint8_t retries = 0;
    size_t remaining = dataSize;

    while (remaining > 0)
    {
        if (retries > MAX_SOCKET_POLL_RETRY)
        {
            lg2::error(
                "Failed to write to output socket for terminus ID {TID}, FileIdentifier {ID} after {RETRY} retries.",
                "TID", tid, "ID", identifier, "RETRY", retries);
            co_return PLDM_ERROR;
        }
        pollfd pollFd{socket, POLLOUT, 0};
        int retval = poll(&pollFd, 1, SOCKET_POLL_TIMEOUT_MS);
        if (retval < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            lg2::error(
                "Failed to poll output socket for terminus ID {TID}, FileIdentifier {ID}, error number - {ERR}.",
                "TID", tid, "ID", identifier, "ERR", errno);
            co_return PLDM_ERROR;
        }
        if (retval == 0)
        {
            lg2::error(
                "Socket poll timeout for terminus ID {TID}, FileIdentifier {ID}.",
                "TID", tid, "ID", identifier);
            retries++;
            continue;
        }
        if (pollFd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            lg2::error(
                "Output socket closed for terminus ID {TID}, FileIdentifier {ID}, events {EVENTS}.",
                "TID", tid, "ID", identifier, "EVENTS", pollFd.revents);
            co_return PLDM_ERROR;
        }
        if (pollFd.revents & POLLOUT)
        {
            auto bytes = send(socket, data + (dataSize - remaining), remaining,
                              MSG_NOSIGNAL);
            if (bytes < 0)
            {
                const int sendErrno = errno;
                if (sendErrno == EINTR)
                {
                    continue;
                }
                if (sendErrno == EAGAIN || sendErrno == EWOULDBLOCK)
                {
                    retries++;
                    continue;
                }
                lg2::error(
                    "Failed to write to output socket for terminus ID {TID}, FileIdentifier {ID}, error number - {ERR}.",
                    "TID", tid, "ID", identifier, "ERR", sendErrno);
                co_return PLDM_ERROR;
            }
            if (bytes == 0)
            {
                lg2::error(
                    "Zero-byte socket write for terminus ID {TID}, FileIdentifier {ID}.",
                    "TID", tid, "ID", identifier);
                co_return PLDM_ERROR;
            }
            remaining -= static_cast<size_t>(bytes);
            retries = 0;
        }
        else
        {
            lg2::debug(
                "Output socket is not writable for terminus ID {TID}, FileIdentifier {ID}.",
                "TID", tid, "ID", identifier);
            retries++;
            continue;
        }
    }

    co_return PLDM_SUCCESS;
}

exec::task<int> FileDescriptor::readFileBySection(
    DfOpenFD fd, SectionOffset offset, SectionOffset length,
    std::vector<uint8_t>& readBuffer)
{
    int rc;

    TransferOp transferOp = PLDM_XFER_FIRST_PART;
    DfOpenFD transferCtx = fd;
    TransferHandle dataTransferHandle = 0;
    SectionOffset requestSectionOffset = offset;
    SectionOffset requestSectionLength = length;

    bool isEnd = false;

    while (!isEnd)
    {
        Checksum dataIntegrityChecksum = 0;

        const struct pldm_base_multipart_receive_req reqData = {
            PLDM_FILE,          transferOp,           transferCtx,
            dataTransferHandle, requestSectionOffset, requestSectionLength};
        struct pldm_base_multipart_receive_resp respData = {};

        rc = co_await sendMultipartReceive(terminusManager, tid, &reqData,
                                           &respData, dataIntegrityChecksum);

        if (rc)
        {
            lg2::error(
                "Failed to send MultipartReceive request for {LEN} bytes at offset {OFF} for TID {TID}, file descriptor {FD} with error {RC}",
                "LEN", requestSectionLength, "OFF", requestSectionOffset, "TID",
                tid, "FD", fd, "RC", rc);
            co_return rc;
        }

        std::vector<uint8_t> buffer;
        if (respData.data.length > 0)
        {
            if (respData.data.ptr == nullptr)
            {
                lg2::error(
                    "MultipartReceive returned a missing payload for terminus ID {TID}, file descriptor {FD}.",
                    "TID", tid, "FD", fd);
                co_return PLDM_ERROR;
            }
            buffer.insert(buffer.end(), respData.data.ptr,
                          respData.data.ptr + respData.data.length);
        }

        if (buffer.size())
        {
            readBuffer.insert(readBuffer.end(), buffer.begin(), buffer.end());
        }

        if (respData.transfer_flag !=
            PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_ACK_COMPLETION)
        {
            auto calculatedCRC =
                pldm_edac_crc32(readBuffer.data(), readBuffer.size());
            if (calculatedCRC != dataIntegrityChecksum)
            {
                lg2::info(
                    "Checksum mismatched while sending MultipartReceive request for TID {TID}, file descriptor {FD}.\nRe-requesting current part",
                    "TID", tid, "FD", fd);

                readBuffer.resize(readBuffer.size() - buffer.size());
                // dataTransferHandle remains the handle previously requested
                transferOp = PLDM_XFER_CURRENT_PART;
                requestSectionOffset = 0;
                requestSectionLength = 0;
                continue;
            }

            rc = co_await writeDataToSocket(buffer.size(), buffer.data(),
                                            sockets[SERVER_IDX]);
            if (rc)
            {
                lg2::error(
                    "Failed to write part data of {LEN} bytes to the output socket {SOCKET} with error {RC}",
                    "LEN", buffer.size(), "SOCKET", sockets[SERVER_IDX], "RC",
                    rc);
                co_return rc;
            }
        }

        switch (respData.transfer_flag)
        {
            case PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_START:
            case PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_MIDDLE:
                transferOp = PLDM_XFER_NEXT_PART;
                dataTransferHandle = respData.next_transfer_handle;
                requestSectionOffset = 0;
                requestSectionLength = 0;
                break;
            case PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_END:
            case PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_START_AND_END:
                // Leave the decision of whether to request another section
                // to the caller
                transferOp = PLDM_XFER_COMPLETE;
                dataTransferHandle = 0;
                requestSectionOffset = 0;
                requestSectionLength = 0;
                break;
            case PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_ACK_COMPLETION:
                isEnd = true;
                break;
            default:
                lg2::error(
                    "Received bad transfer flag {FLAG} while sending MultipartReceive request for TID {TID}, file descriptor {FD}.",
                    "FLAG", respData.transfer_flag, "TID", tid, "FD", fd);
                co_return PLDM_ERROR;
        }
    }

    co_return rc;
}

exec::task<int> FileDescriptor::readFileByLength(DfOpenFD fd, size_t offset,
                                                 size_t readLength)
{
    std::vector<uint8_t> readBuffer{};
    int rc;

    if (isRegular)
    {
        rc = co_await readFileBySection(fd, offset, readLength, readBuffer);
        if (rc)
        {
            lg2::error(
                "Failed to read {LEN} bytes at offset {OFF} from terminus ID {TID}, file descriptor {FD}, error {RC}.",
                "LEN", readLength, "OFF", offset, "TID", tid, "FD", fd, "RC",
                rc);
            co_return PLDM_ERROR;
        }
        co_return PLDM_SUCCESS;
    }

    PartSize reqPartSize = REQUESTER_PART_SIZE;
    PartSize respPartSize = 0;
    bitfield8_t reqProtocol[8];
    memset(&reqProtocol, 0, sizeof(reqProtocol));
    reqProtocol[0].bits.bit0 = 1;
    reqProtocol[0].bits.bit7 = 1;

    struct pldm_base_negotiate_transfer_params_req negoParamsReq = {};
    negoParamsReq.requester_part_size = reqPartSize;
    memcpy(negoParamsReq.requester_protocol_support, reqProtocol,
           8 * sizeof(*reqProtocol));
    struct pldm_base_negotiate_transfer_params_resp negoParamsResp = {};
    rc = co_await negotiateTransferParameters(terminusManager, tid,
                                              &negoParamsReq, &negoParamsResp);
    if (rc)
    {
        respPartSize = REQUESTER_PART_SIZE;
        lg2::info(
            "Failed to negotiate part size {PARTSIZE} for terminus ID {TID}, use {DEFAULT}",
            "PARTSIZE", reqPartSize, "TID", tid, "DEFAULT", respPartSize);
    }
    else
    {
        respPartSize = negoParamsResp.responder_part_size;
    }

    rc = 0;
    uint32_t remaining = readLength;
    bool lastRead = false;

    while ((remaining > 0) && !lastRead)
    {
        std::vector<uint8_t> buff{};
        PartSize partSize = respPartSize;

        if (partSize > remaining)
        {
            partSize = remaining;
            lastRead = true;
        }

        rc = co_await readFileBySection(fd, 0, partSize, buff);

        if (rc)
        {
            lg2::error(
                "Failed to read {LEN} bytes at offset 0 from terminus ID {TID}, file descriptor {FD}, error {RC}.",
                "LEN", partSize, "TID", tid, "FD", fd, "RC", rc);
            co_return PLDM_ERROR;
        }

        readBuffer.insert(readBuffer.end(), buff.begin(), buff.end());

        if (buff.size() < partSize)
        {
            lastRead = true;
        }

        remaining -= buff.size();
    }

    co_return PLDM_SUCCESS;
}

exec::task<int> FileDescriptor::readTask(size_t offset, size_t length,
                                         bool exclusivity)
{
    int rc;
    DfOpenFD curFd;
    bitfield16_t reqAttr;
    reqAttr.value = 0;
    if (isRegular)
    {
        reqAttr.bits.bit1 = exclusivity ? 1 : 0;
    }
    else
    {
        reqAttr.bits.bit2 = 1;
    }

    struct pldm_file_df_open_req openReq = {identifier, reqAttr};
    struct pldm_file_df_open_resp openResp = {};
    rc = co_await dfOpen(terminusManager, tid, &openReq, &openResp);
    if (rc)
    {
        lg2::error(
            "Failed to open file session from terminus ID {TID}, FileIdentifier {ID}, error {RC}.",
            "TID", tid, "ID", identifier, "RC", rc);
        co_return rc;
    }

    curFd = openResp.file_descriptor;
    if (isRegular)
    {
        Interval requesterMaxInterval = REQUESTER_MAX_INTERVAL_MS;
        const struct pldm_file_df_heartbeat_req heartBeatReq = {
            curFd, requesterMaxInterval};
        struct pldm_file_df_heartbeat_resp heartBeatResp = {};
        rc = co_await dfHeartbeat(terminusManager, tid, &heartBeatReq,
                                  &heartBeatResp);
    }

    FileSize fileSize = getFileSize();

    int readRc = PLDM_SUCCESS;
    if (fileSize > 0)
    {
        auto readLength = (length > 0) ? length : fileSize;

        readRc = co_await readFileByLength(curFd, offset, readLength);
        if (readRc)
        {
            lg2::error(
                "Failed to read {LEN} bytes at offset {OFF} for terminus ID {TID}, FileIdentifier {ID}, error {RC}.",
                "LEN", readLength, "OFF", offset, "TID", tid, "ID", identifier,
                "RC", readRc);
            // No return here as we still need to call DfClose.
        }
    }

    const struct pldm_file_df_close_req closeReq = {
        curFd, static_cast<bitfield16_t>(0)};
    rc = co_await dfClose(terminusManager, tid, &closeReq);
    if (rc)
    {
        lg2::error(
            "Failed to close file session from terminus ID {TID}, FileIdentifier {ID}, error {RC}.",
            "TID", tid, "ID", identifier, "RC", rc);
    }

    co_return readRc != PLDM_SUCCESS ? readRc : rc;
}

sdbusplus::message::unix_fd FileDescriptor::startFileTransfer(
    size_t offset, size_t length, bool exclusivity)
{
    if (isDirectory)
    {
        elog<NotAllowed>(
            NotAllowedMetaData::REASON("Reading a directory is not allowed"));
    }

    if (exclusivity)
    {
        if ((isRegular && !exReadPermitted) || !isRegular)
        {
            elog<NotAllowed>(NotAllowedMetaData::REASON(
                "File does not support exclusive read"));
        }
    }

    // TODO: Support multiple task handles based on
    // FileMaximumFileDescriptorCount
    if (taskHandle.has_value())
    {
        auto& [scope, rcOpt] = *taskHandle;
        if (!rcOpt.has_value())
        {
            lg2::error(
                "Failed to read from terminus ID {TID}, FileIdentifier {ID} - file is busy.",
                "TID", tid, "ID", identifier);
            elog<Unavailable>();
        }
        stdexec::sync_wait(scope.on_empty());
        taskHandle.reset();
    }

    /* Create a socketpair */
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sockets) == -1)
    {
        lg2::error(
            "Failed to setup socketpair for terminus ID {TID}, FileIdentifier {ID}, error number {ERR}.",
            "TID", tid, "ID", identifier, "ERR", errno);
        elog<InternalFailure>();
    }

    if (setsockopt(sockets[SERVER_IDX], SOL_SOCKET, SO_SNDBUF,
                   &FILE_TRANSFER_SOCKET_BUFFER_SIZE,
                   sizeof(FILE_TRANSFER_SOCKET_BUFFER_SIZE)) < 0)
    {
        lg2::debug(
            "Failed to set send buffer size for terminus ID {TID}, FileIdentifier {ID}, error number {ERR}.",
            "TID", tid, "ID", identifier, "ERR", errno);
    }

    if (setsockopt(sockets[CLIENT_IDX], SOL_SOCKET, SO_RCVBUF,
                   &FILE_TRANSFER_SOCKET_BUFFER_SIZE,
                   sizeof(FILE_TRANSFER_SOCKET_BUFFER_SIZE)) < 0)
    {
        lg2::debug(
            "Failed to set receive buffer size for terminus ID {TID}, FileIdentifier {ID}, error number {ERR}.",
            "TID", tid, "ID", identifier, "ERR", errno);
    }

    sdbusplus::message::unix_fd clientFd(sockets[CLIENT_IDX]);
    sockets[CLIENT_IDX] = -1;

    auto& [scope, rcOpt] = taskHandle.emplace();
    scope.spawn(
        readTask(offset, length, exclusivity) |
            stdexec::upon_error([this](std::exception_ptr error) noexcept {
                try
                {
                    if (!error)
                    {
                        lg2::error(
                            "File transfer task failed for terminus ID {TID}, FileIdentifier {ID} without an error detail.",
                            "TID", tid, "ID", identifier);
                        return PLDM_ERROR;
                    }
                    try
                    {
                        std::rethrow_exception(error);
                    }
                    catch (const std::exception& e)
                    {
                        lg2::error(
                            "File transfer task failed for terminus ID {TID}, FileIdentifier {ID}: {ERROR}",
                            "TID", tid, "ID", identifier, "ERROR", e);
                    }
                    catch (...)
                    {
                        lg2::error(
                            "File transfer task failed for terminus ID {TID}, FileIdentifier {ID} with an unknown error.",
                            "TID", tid, "ID", identifier);
                    }
                }
                catch (...)
                {
                    // A logging failure must not escape the task receiver.
                }
                return PLDM_ERROR;
            }) |
            stdexec::upon_stopped([this]() noexcept {
                try
                {
                    lg2::error(
                        "File transfer task stopped for terminus ID {TID}, FileIdentifier {ID}.",
                        "TID", tid, "ID", identifier);
                }
                catch (...)
                {
                    // Complete the task even if logging fails.
                }
                return PLDM_ERROR;
            }) |
            stdexec::then([this, &rcOpt](int rc) noexcept {
                try
                {
                    lg2::info(
                        "Read finishes for terminus ID {TID}, FileIdentifier {ID}, return code {RC}.",
                        "TID", tid, "ID", identifier, "RC", rc);
                }
                catch (...)
                {
                    // Complete the task and close its socket even if logging fails.
                }
                rcOpt.emplace(rc);
                if (sockets[SERVER_IDX] >= 0)
                {
                    close(sockets[SERVER_IDX]);
                    sockets[SERVER_IDX] = -1;
                }
            }),
        exec::default_task_context<void>(stdexec::inline_scheduler{}));

    return clientFd;
}

sdbusplus::message::unix_fd FileDescriptor::open(size_t offset, size_t length,
                                                 bool exclusivity)
{
    // sd-bus duplicates this FD while serializing the method reply. Keep the
    // original alive until the next Open, after that reply has been sent.
    auto clientFd = startFileTransfer(offset, length, exclusivity);
    dbusClientFd.reset();
    dbusClientFd.emplace(clientFd.fd);
    return clientFd;
}

FileSize FileDescriptor::getFileSize() const
{
    if (!sizeSensor)
    {
        sizeSensor =
            terminusManager.getFileSizeMonitoringSensor(tid, entityInfo);
        if (!sizeSensor)
        {
            /*
             * DSP0242 v1.0.0 Section 8.8.3 File Size Monitoring Sensor:
             * If the File PDR does not have an associated File Size Monitoring
             * Sensor, then the File Size is the number of bytes indicated by
             * the File PDR FileMaximumSize field;
             */
            lg2::error(
                "No size sensor available for terminus ID {TID}, FileIdentifier {ID}",
                "TID", tid, "ID", identifier);
            return maxSize;
        }
    }

    double fileSize = sizeSensor->getSensorValue();
    if (std::isfinite(fileSize))
    {
        return fileSize;
    }
    else
    {
        return maxSize;
    }
}
} // namespace platform_mc
} // namespace pldm
