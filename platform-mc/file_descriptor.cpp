#include "file_descriptor.hpp"

#include "file_cmds.hpp"
#include "file_transfer_wait.hpp"
#include "regular_file_transfer.hpp"

#include <libpldm/edac.h>
#include <libpldm/entity.h>
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
        elog<Unavailable>();
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

exec::task<int> FileDescriptor::keepAlive()
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline || !available || !sessionOpen ||
        negotiatedInterval == 0 ||
        now - lastActivity >= std::chrono::milliseconds(negotiatedInterval))
    {
        co_return PLDM_ERROR_NOT_READY;
    }
    if (now - lastActivity < std::chrono::milliseconds(negotiatedInterval / 2))
    {
        co_return PLDM_SUCCESS;
    }
    const pldm_file_df_heartbeat_req request{activeDescriptor,
                                             REQUESTER_MAX_INTERVAL_MS};
    pldm_file_df_heartbeat_resp response{};
    const auto sentAt = std::chrono::steady_clock::now();
    const auto previousInterval = negotiatedInterval;
    auto rc = co_await dfHeartbeat(terminusManager, tid, &request, &response);
    if (!rc && (!available || std::chrono::steady_clock::now() - lastActivity >=
                                  std::chrono::milliseconds(previousInterval)))
    {
        rc = PLDM_ERROR_NOT_READY;
    }
    if (rc)
    {
        co_return rc;
    }
    negotiatedInterval = std::min<uint32_t>(REQUESTER_MAX_INTERVAL_MS,
                                            response.responder_max_interval);
    if (negotiatedInterval < 10)
    {
        // Local scheduling support limit, not a DMTF minimum.
        co_return PLDM_ERROR_NOT_READY;
    }
    lastActivity = sentAt;
    co_return PLDM_SUCCESS;
}

exec::task<int> FileDescriptor::waitForStorage()
{
    // Signal data EOF while retaining the PLDM session until the sink has
    // synchronized its file. This is an internal API, not a new wire protocol.
    if (shutdown(sockets[SERVER_IDX], SHUT_WR) < 0)
    {
        co_return PLDM_ERROR;
    }
    const auto progressDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (storageCompletion.wait_for(std::chrono::seconds(0)) !=
           std::future_status::ready)
    {
        auto rc = co_await keepAlive();
        if (rc || std::chrono::steady_clock::now() >= progressDeadline)
        {
            co_return rc ? rc : PLDM_ERROR_NOT_READY;
        }
        if (co_await file_transfer::Yield{})
        {
            co_return PLDM_ERROR;
        }
    }
    co_return storageCompletion.get();
}

exec::task<int> FileDescriptor::writeDataToSocket(
    size_t dataSize, const uint8_t* const data, int socket)
{
    size_t written = 0;
    auto progressDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (written < dataSize)
    {
        auto rc = co_await keepAlive();
        if (rc || std::chrono::steady_clock::now() >= progressDeadline)
        {
            co_return PLDM_ERROR;
        }
        auto count = send(socket, data + written, dataSize - written,
                          MSG_DONTWAIT | MSG_NOSIGNAL);
        if (count > 0)
        {
            written += static_cast<size_t>(count);
            progressDeadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(5);
            continue;
        }
        if (count < 0 &&
            (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
        {
            // Yield even on EINTR: repeated signals cannot monopolize the loop.
            if (co_await file_transfer::Yield{})
            {
                co_return PLDM_ERROR;
            }
            continue;
        }
        co_return PLDM_ERROR; // zero send, closed peer, or another socket error
    }
    co_return PLDM_SUCCESS;
}

exec::task<int> FileDescriptor::readFileBySection(
    DfOpenFD fd, SectionOffset offset, SectionOffset length,
    std::vector<uint8_t>& readBuffer, bool finalSection)
{
    bool delivered = false;
    file_transfer::Section section(length, negotiatedPartSize);
    while (true)
    {
        auto rc = co_await keepAlive();
        if (rc)
        {
            co_return rc;
        }
        const auto operation = section.operation();
        if (operation == PLDM_XFER_COMPLETE && finalSection &&
            storageCompletion.valid() && !delivered)
        {
            rc = co_await writeDataToSocket(section.bytes().size(),
                                            section.bytes().data(),
                                            sockets[SERVER_IDX]);
            if (!rc)
            {
                rc = co_await waitForStorage();
            }
            if (rc)
            {
                co_return rc; // Caller aborts before the final section ACK.
            }
            delivered = true;
        }
        const pldm_base_multipart_receive_req request{
            PLDM_FILE,
            operation,
            fd,
            section.handle(),
            operation == PLDM_XFER_FIRST_PART ? offset : 0,
            operation == PLDM_XFER_FIRST_PART ? length : 0};
        pldm_base_multipart_receive_resp response{};
        uint32_t checksum = 0;
        const auto sentAt = std::chrono::steady_clock::now();
        rc = co_await sendMultipartReceive(terminusManager, tid, &request,
                                           &response, checksum);
        if (rc || !available)
        {
            co_return rc ? rc : PLDM_ERROR_NOT_READY;
        }
        if (std::chrono::steady_clock::now() - lastActivity >=
            std::chrono::milliseconds(negotiatedInterval))
        {
            co_return PLDM_ERROR_NOT_READY;
        }
        lastActivity = sentAt;
        if (response.data.length && !response.data.ptr)
        {
            co_return PLDM_ERROR_INVALID_DATA;
        }
        auto outcome = section.accept(
            response.transfer_flag, response.next_transfer_handle,
            std::span<const uint8_t>(response.data.ptr, response.data.length),
            checksum);
        if (outcome == file_transfer::Section::Outcome::invalid)
        {
            co_return PLDM_ERROR_INVALID_DATA;
        }
        if (outcome == file_transfer::Section::Outcome::complete)
        {
            if (!delivered)
            {
                readBuffer.assign(section.bytes().begin(),
                                  section.bytes().end());
            }
            co_return PLDM_SUCCESS;
        }
    }
}

exec::task<int> FileDescriptor::readFileByLength(DfOpenFD fd, size_t offset,
                                                 size_t length)
{
    while (length > 0)
    {
        const auto count = static_cast<uint32_t>(
            std::min<size_t>(length, file_transfer::Section::maximumSection));
        std::vector<uint8_t> section;
        auto rc = co_await readFileBySection(fd, offset, count, section,
                                             count == length);
        if (rc)
        {
            // DSP0240 Table 17: abort uses zero handle/offset/length.
            const pldm_base_multipart_receive_req abort{
                PLDM_FILE, PLDM_XFER_ABORT, fd, 0, 0, 0};
            pldm_base_multipart_receive_resp ignored{};
            uint32_t checksum = 0;
            // Transport may be gone; cleanup failure must not replace root
            // cause.
            if (available)
            {
                co_await sendMultipartReceive(terminusManager, tid, &abort,
                                              &ignored, checksum);
            }
            co_return rc;
        }
        rc = co_await writeDataToSocket(section.size(), section.data(),
                                        sockets[SERVER_IDX]);
        if (rc)
        {
            co_return rc;
        }
        offset += count;
        length -= count;
    }
    co_return PLDM_SUCCESS;
}

exec::task<int> FileDescriptor::readTask(size_t offset, size_t length,
                                         bool exclusivity)
{
    deadline = std::chrono::steady_clock::now() + std::chrono::minutes(2);
    // Fresh negotiation per transfer avoids a stale negotiation cache on reset.
    pldm_base_negotiate_transfer_params_req negotiate{};
    negotiate.requester_part_size = REQUESTER_PART_SIZE;
    negotiate.requester_protocol_support[0].bits.bit7 = 1;
    pldm_base_negotiate_transfer_params_resp parameters{};
    auto rc = co_await negotiateTransferParameters(terminusManager, tid,
                                                   &negotiate, &parameters);
    auto partSize = file_transfer::negotiatePartSize(
        REQUESTER_PART_SIZE, parameters.responder_part_size,
        parameters.responder_protocol_support[0].bits.bit7);
    if (rc || !available || !partSize)
    {
        co_return rc ? rc : PLDM_ERROR_INVALID_DATA;
    }
    negotiatedPartSize = *partSize;

    // Size/range validation happens before the remote file is opened.
    const auto size = getFileSize();
    auto count = file_transfer::readLength(size, offset, length);
    if (!count)
    {
        co_return PLDM_ERROR_INVALID_DATA;
    }
    bitfield16_t attributes{};
    attributes.bits.bit1 = exclusivity;
    const pldm_file_df_open_req open{identifier, attributes};
    pldm_file_df_open_resp opened{};
    rc = co_await dfOpen(terminusManager, tid, &open, &opened);
    if (rc || !available)
    {
        co_return rc ? rc : PLDM_ERROR_NOT_READY;
    }
    activeDescriptor = opened.file_descriptor;
    sessionOpen = true;
    int readRc = PLDM_SUCCESS;
    try
    {
        const pldm_file_df_heartbeat_req heartbeat{activeDescriptor,
                                                   REQUESTER_MAX_INTERVAL_MS};
        pldm_file_df_heartbeat_resp response{};
        const auto sentAt = std::chrono::steady_clock::now();
        readRc =
            co_await dfHeartbeat(terminusManager, tid, &heartbeat, &response);
        negotiatedInterval = std::min<uint32_t>(
            REQUESTER_MAX_INTERVAL_MS, response.responder_max_interval);
        lastActivity = sentAt;
        if (!readRc && negotiatedInterval < 10)
        {
            readRc = PLDM_ERROR_NOT_READY;
        }
        if (!readRc)
        {
            readRc =
                co_await readFileByLength(activeDescriptor, offset, *count);
            if (!readRc && *count == 0 && storageCompletion.valid())
            {
                readRc = co_await waitForStorage();
            }
        }
    }
    catch (...)
    {
        // Exit the catch before awaiting cleanup (C++ coroutine restriction).
        readRc = PLDM_ERROR;
    }
    const pldm_file_df_close_req close{activeDescriptor, bitfield16_t{}};
    rc = PLDM_ERROR_NOT_READY;
    if (available)
    {
        rc = co_await dfClose(terminusManager, tid, &close);
    }
    sessionOpen = false;
    co_return !available ? PLDM_ERROR_NOT_READY : (readRc ? readRc : rc);
}

sdbusplus::message::unix_fd FileDescriptor::startFileTransfer(
    size_t offset, size_t length, bool exclusivity)
{
    return startRegularTransfer(offset, length, exclusivity).stream;
}

FileDescriptor::Transfer FileDescriptor::startRegularTransfer(
    size_t offset, size_t length, bool exclusivity, bool requireStorage)
{
    if (!available)
    {
        elog<Unavailable>();
    }
    if (!isRegular || isDirectory)
    {
        elog<NotAllowed>(NotAllowedMetaData::REASON(
            "Only regular files are supported by this transfer client"));
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

    auto stored =
        requireStorage ? std::make_shared<std::promise<int>>() : nullptr;
    storageCompletion =
        stored ? stored->get_future().share() : std::shared_future<int>{};

    /* Create a socketpair */
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
                   sockets) == -1)
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

    completion = std::make_shared<std::promise<int>>();
    auto terminal = completion->get_future().share();
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
                completion->set_value(rc);
                if (sockets[SERVER_IDX] >= 0)
                {
                    close(sockets[SERVER_IDX]);
                    sockets[SERVER_IDX] = -1;
                }
            }),
        exec::default_task_context<void>(stdexec::inline_scheduler{}));

    return {clientFd, std::move(terminal), std::move(stored)};
}

sdbusplus::message::unix_fd FileDescriptor::open(size_t offset, size_t length,
                                                 bool exclusivity)
{
    // sd-bus duplicates this FD while serializing the method reply. Keep the
    // original alive until the next Open, after that reply has been sent.
    auto clientFd = startFileTransfer(offset, length, exclusivity);
    dbusClientFd.reset();
    dbusClientFd = std::make_unique<pldm::utils::CustomFD>(clientFd.fd);
    return clientFd;
}

FileSize FileDescriptor::getFileSize() const
{
    if (!available)
    {
        elog<Unavailable>();
    }
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

    const auto size =
        file_transfer::validFileSize(sizeSensor->getSensorValue());
    if (!size)
    {
        elog<Unavailable>();
    }
    return *size;
}
} // namespace platform_mc
} // namespace pldm
