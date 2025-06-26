#include "file_descriptor.hpp"

#include "file_cmds.hpp"

#include <libpldm/entity.h>
#include <sys/socket.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

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

FileDescriptor::FileDescriptor(sdbusplus::bus_t& bus, const std::string& path,
                               const pldm_tid_t tid, const bool fileDisabled,
                               std::shared_ptr<pldm_file_descriptor_pdr> pdr,
                               TerminusManager& terminusManager) :
    FileInterface(bus, path.c_str()), tid(tid), terminusManager(terminusManager)
{
    if (!pdr || fileDisabled)
    {
        lg2::error(
            "Terminus ID {TID}: File Descriptor Constructor does not have valid pdr ptr.",
            "TID", tid);
    }

    auto entityId = pdr->container.entity_type & 0x7FFF;
    isDirectory = (entityId == PLDM_ENTITY_DEVICE_FILE_DIRECTORY);
    isRegular = (pdr->file_capabilities.bits.bit3 == 0);
    exReadPermitted = (pdr->file_capabilities.bits.bit0 == 1);
    identifier = pdr->file_identifier;
    supDirIdentifier = pdr->superior_directory_file_identifier;
    maxSize = pdr->file_maximum_size;
    maxFdCount = pdr->file_maximum_file_descriptor_count;
    std::string fileName((const char*)(pdr->file_name.ptr),
                         pdr->file_name.length);
    name(fileName);
    source(SourceType::System);
    if (pdr->oem_file_classification)
    {
        oemClassName.assign(
            (const char*)(pdr->oem_file_classification_name.ptr),
            pdr->oem_file_classification_name.length);
    }
    setPurpose(static_cast<FileClassification>(pdr->file_classification));
}

void FileDescriptor::setPurpose(const FileClassification& classification)
{
    switch (classification)
    {
        case FileClassification::OEM:
            purpose(PurposeType::OEM);
            break;
        case FileClassification::BootLog:
            purpose(PurposeType::BootLog);
            break;
        case FileClassification::SerialTxFIFO:
            purpose(PurposeType::SerialTxFIFO);
            break;
        case FileClassification::SerialRxFIFO:
            purpose(PurposeType::SerialRxFIFO);
            break;
        case FileClassification::DiagnosticLog:
            purpose(PurposeType::DiagnosticLog);
            break;
        case FileClassification::CrashDumpFile:
            purpose(PurposeType::CrashDump);
            break;
        case FileClassification::SecurityLog:
            purpose(PurposeType::Security);
            break;
        case FileClassification::FRUDataFile:
            purpose(PurposeType::FRU);
            break;
        case FileClassification::TelemetryDataFile:
            purpose(PurposeType::TelemetryDataFile);
            break;
        case FileClassification::TelemetryDataLog:
            purpose(PurposeType::TelemetryDataLog);
            break;
        case FileClassification::FileDirectory:
            purpose(PurposeType::Directory);
            break;
        case FileClassification::OtherLog:
        case FileClassification::OtherFile:
        default:
            purpose(PurposeType::Unknown);
            break;
    }
}

exec::task<int> FileDescriptor::writeDataToSocket(
    const uint32_t& dataSize, const uint8_t* const data, const int& socket,
    const int& maxFd)
{
    fd_set wfd;
    struct timeval tv;
    uint8_t retries = 0;
    uint32_t remaining = dataSize;

    while (remaining > 0)
    {
        if (retries == 5)
        {
            lg2::error(
                "Failed to write to output socket for terminus ID {TID}, FileIdentifier {ID} after {RETRY} retries.",
                "TID", tid, "ID", identifier, "RETRY", retries);
            co_return PLDM_ERROR;
        }
        FD_ZERO(&wfd);
        FD_SET(socket, &wfd);
        tv.tv_sec = 0;
        tv.tv_usec = 200000;

        int retval = select(maxFd + 1, nullptr, &wfd, nullptr, &tv);
        if (retval < 0)
        {
            lg2::error(
                "Failed to select output socket for terminus ID {TID}, FileIdentifier {ID}, error number - {ERR}.",
                "TID", tid, "ID", identifier, "ERR", errno);
            co_return PLDM_ERROR;
        }
        if (retval == 0)
        {
            lg2::error(
                "Select timeout for terminus ID {TID}, FileIdentifier {ID}, error number - {ERR}.",
                "TID", tid, "ID", identifier, "ERR", errno);
            retries++;
            continue;
        }
        if ((retval > 0) && (FD_ISSET(socket, &wfd)))
        {
            int bytes = write(socket, data, remaining);
            if (bytes < 0)
            {
                lg2::error(
                    "Failed to write to output socket for terminus ID {TID}, FileIdentifier {ID}, error number - {ERR}.",
                    "TID", tid, "ID", identifier, "ERR", errno);
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    retries++;
                    continue;
                }
                co_return PLDM_ERROR;
            }
            remaining -= bytes;
            retries = 0;
        }
        else
        {
            lg2::debug(
                "Write socket FD is not set for terminus ID {TID}, FileIdentifier {ID}.",
                "TID", tid, "ID", identifier);
            retries++;
            continue;
        }
    }
    co_return PLDM_SUCCESS;
}

exec::task<int> FileDescriptor::readFileToSocket(
    const FD& fd, const SectionOffset& offset, const SectionOffset& length,
    std::vector<uint8_t>& readBuffer)
{
    int rc;

    TransferOp transferOperation = PLDM_XFER_FIRST_PART;
    FD transferCtx = fd;
    TransferHandle dataTransferHandle = 0;
    SectionOffset requestSectionOffset = offset;
    SectionOffset requestSectionLength = length;

    bool isEnd = false;

    int maxFd = (sockets[0] > sockets[1]) ? sockets[0] : sockets[1];

    while (!isEnd)
    {
        uint8_t completionCode = 0;
        TransferFlag transferFlag = 0;
        TransferHandle nextDataTransferHandle = 0;
        Checksum dataIntegrityChecksum = 0;

        std::vector<uint8_t> buffer;

        rc = co_await sendMultipartReceive(
            terminusManager, tid, transferOperation, transferCtx,
            dataTransferHandle, requestSectionOffset, requestSectionLength,
            completionCode, transferFlag, nextDataTransferHandle, buffer,
            dataIntegrityChecksum);
        if (rc || completionCode)
        {
            lg2::error(
                "Failed to send MultipartReceive request for {LEN} bytes at offset {OFF} for TID {TID}, file descriptor {FD} with error {RC}, complete code {CC}",
                "LEN", requestSectionLength, "OFF", requestSectionOffset, "TID",
                tid, "FD", fd, "RC", rc, "CC", completionCode);
            co_return rc;
        }

        if (buffer.size() > 0)
        {
            readBuffer.insert(readBuffer.end(), buffer.begin(), buffer.end());
        }

        auto calculatedCRC =
            pldm_edac_crc32(readBuffer.data(), readBuffer.size());
        if (calculatedCRC != dataIntegrityChecksum)
        {
            lg2::info(
                "Checksum mismatched while sending MultipartReceive request for TID {TID}, file descriptor {FD}.\nRe-requesting current part",
                "TID", tid, "FD", fd);

            readBuffer.resize(readBuffer.size() - buffer.size());
            transferOperation = PLDM_XFER_CURRENT_PART;
            dataTransferHandle = 0;
            requestSectionOffset = 0;
            requestSectionLength = 0;
            continue;
        }

        if (transferFlag !=
            PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_ACK_COMPLETION)
        {
            rc = co_await writeDataToSocket(buffer.size(), buffer.data(),
                                            sockets[0], maxFd);
            if (rc)
            {
                lg2::error(
                    "Failed to write part data of {LEN} bytes to the output socket {SOCKET} with error {RC}",
                    "LEN", buffer.size(), "SOCKET", sockets[0], "RC", rc);
                co_return rc;
            }
        }

        switch (transferFlag)
        {
            case PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_START:
            case PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_MIDDLE:
                transferOperation = PLDM_XFER_NEXT_PART;
                dataTransferHandle = nextDataTransferHandle;
                requestSectionOffset = 0;
                requestSectionLength = 0;
                break;
            case PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_END:
            case PLDM_BASE_MULTIPART_RECEIVE_TRANSFER_FLAG_START_AND_END:
                // Leave the decision of whether to request another section
                // to the caller
                transferOperation = PLDM_XFER_COMPLETE;
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
                    "FLAG", transferFlag, "TID", tid, "FD", fd);
                co_return PLDM_ERROR;
        }
    }
    co_return rc;
}

exec::task<int> FileDescriptor::readFile(
    const uint16_t& fd, const uint32_t& offset, const uint32_t& readSize)
{
    std::vector<uint8_t> readBuffer{};
    int rc;

    if (isRegular)
    {
        rc = co_await readFileToSocket(fd, offset, readSize, readBuffer);
        if (rc)
        {
            lg2::error(
                "Failed to read {LEN} bytes at offset {OFF} from terminus ID {TID}, file descriptor {FD}, error {RC}.",
                "LEN", readSize, "OFF", offset, "TID", tid, "FD", fd, "RC", rc);
            co_return PLDM_ERROR;
        }
    }
    else
    {
        PartSize reqPartSize = 0x100;
        PartSize respPartSize = 0;
        bitfield8_t reqProtocol[8], respProtocol[8];
        reqProtocol[0].bits.bit0 = 1;
        reqProtocol[0].bits.bit7 = 1;

        rc = co_await negotiateTransferParameters(
            terminusManager, tid, reqPartSize, reqProtocol, respPartSize,
            respProtocol);
        if (rc)
        {
            respPartSize = 0x100;
            lg2::info(
                "Failed to negotiate part size {PARTSIZE} for terminus ID {TID}, use {DEFAULT}",
                "PARTSIZE", reqPartSize, "TID", tid, "DEFAULT", respPartSize);
        }

        rc = 0;
        uint32_t remaining = readSize;
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

            rc = co_await readFileToSocket(fd, 0, partSize, buff);

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
    }
    co_return PLDM_SUCCESS;
}

exec::task<int> FileDescriptor::readTask(uint32_t offset, uint32_t length,
                                         bool exclusivity)
{
    int rc;
    uint16_t curFd;
    bitfield16_t reqAttr;
    reqAttr.value = 0;
    if (isRegular)
    {
        reqAttr.bits.bit1 = exclusivity ? 1 : 0;
    }
    else
    {
        offset = 0;
        reqAttr.bits.bit2 = 1;
    }

    rc = co_await dfOpen(terminusManager, tid, identifier, reqAttr, curFd);
    if (rc == PLDM_SUCCESS)
    {
        Interval requesterMaxInterval = 500;
        Interval responderMaxInterval = 0;
        rc = co_await dfHeartbeat(terminusManager, tid, curFd,
                                  requesterMaxInterval, responderMaxInterval);

        if (rc == PLDM_SUCCESS)
        {
            // [ChauLy] TODO: Support getting file size from File Size Numeric
            // Sensor
            rc = co_await readFile(curFd, offset, length);
            if (rc)
            {
                lg2::error(
                    "Failed to read {LEN} bytes at offset {OFF} for terminus ID {TID}, FileIdentifier {ID}, error {RC}.",
                    "LEN", length, "OFF", offset, "TID", tid, "ID", identifier,
                    "RC", rc);
            }
        }
        else
        {
            lg2::error(
                "Failed to get file size from terminus ID {TID}, FileIdentifier {ID}, error {RC}.",
                "TID", tid, "ID", identifier, "RC", rc);
        }
    }
    else
    {
        lg2::error(
            "Failed to open file session from terminus ID {TID}, FileIdentifier {ID}, error {RC}.",
            "TID", tid, "ID", identifier, "RC", rc);
        co_return rc;
    }

    rc = co_await dfClose(terminusManager, tid, curFd, (bitfield16_t)0);
    if (rc)
    {
        lg2::error(
            "Failed to close file session from terminus ID {TID}, FileIdentifier {ID}, error {RC}.",
            "TID", tid, "ID", identifier, "RC", rc);
    }
    co_return rc;
}

sdbusplus::message::unix_fd FileDescriptor::read(
    uint32_t offset, uint32_t length, bool exclusivity)
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

    /* Create a socketpair */
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sockets) == -1)
    {
        lg2::error(
            "Failed to setup socketpair for terminus ID {TID}, FileIdentifier {ID}, error number {ERR}.",
            "TID", tid, "ID", identifier, "ERR", errno);
        elog<InternalFailure>();
    }

    // [ChauLy] TODO: Support multiple task handles based on
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
    auto& [scope, rcOpt] = taskHandle.emplace();
    scope.spawn(
        readTask(offset, length, exclusivity) | stdexec::then([&](int rc) {
            lg2::info(
                "Read finishes for terminus ID {TID}, FileIdentifier {ID}, return code {RC}.",
                "TID", tid, "ID", identifier, "RC", rc);
            rcOpt.emplace(rc);
            close(sockets[0]);
            close(sockets[0]);
            sockets[0] = 0;
            sockets[1] = 0;
        }),
        exec::default_task_context<void>(exec::inline_scheduler{}));
    return sockets[1];
}
} // namespace platform_mc
} // namespace pldm
