#pragma once

#include "common/transport.hpp"
#include "common/utils.hpp"
#include "terminus.hpp"
#include "terminus_manager.hpp"

#include <libpldm/base.h>
#include <libpldm/file.h>

#include <phosphor-logging/lg2.hpp>

#include <functional>

namespace pldm
{

namespace platform_mc
{

namespace file_cmds
{
using namespace pldm::file_transfer;

/** @brief Send DfOpen command to file host
 *
 *  @param[in] terminusManager - TerminusManager object
 *  @param[in] tid - Destination TID
 *  @param[in] fileId - FileIdentifier
 *  @param[in] openAttr - DfOpenAttribute
 *  @param[out] fd - FileDescriptor
 */
exec::task<int> dfOpen(TerminusManager& terminusManager, const pldm_tid_t& tid,
                       const FileID& fileId, const bitfield16_t& openAttr,
                       FD& fd);

/** @brief Send DfClose command to file host
 *
 *  @param[in] terminusManager - TerminusManager object
 *  @param[in] tid - Destination TID
 *  @param[in] fd - FileDescriptor
 *  @param[in] closeOptions - DfCloseOptions
 */
exec::task<int> dfClose(TerminusManager& terminusManager, const pldm_tid_t& tid,
                        const FD& fd, const bitfield16_t& closeOptions);

/** @brief Send DfHeartbeat command to file host
 *
 *  @param[in] terminusManager - TerminusManager object
 *  @param[in] tid - Destination TID
 *  @param[in] fd - FileDescriptor
 *  @param[in] reqMaxInterval - RequesterMaxInterval
 *  @param[out] respMaxInterval - ResponderMaxInterval
 */
exec::task<int> dfHeartbeat(
    TerminusManager& terminusManager, const pldm_tid_t& tid, const FD& fd,
    const Interval& reqMaxInterval, Interval& respMaxInterval);

/** @brief Send NegotiateTransferParameters command to file host
 *
 *  @param[in] terminusManager - TerminusManager object
 *  @param[in] tid - Destination TID
 *  @param[in] reqPartSize - RequesterPartSize
 *  @param[in] reqProtocol - RequesterProtocolSupport
 *  @param[out] respPartSize - ResponderPartSize
 *  @param[out] respProtocol - ResponderProtocolSupport
 */
exec::task<int> negotiateTransferParameters(
    TerminusManager& terminusManager, const pldm_tid_t& tid,
    const PartSize& reqPartSize, const bitfield8_t* const reqProtocol,
    PartSize& respPartSize, bitfield8_t* const respProtocol);

/** @brief Send MultipartReceive command to file host
 *
 *  @param[in] terminusManager - TerminusManager object
 *  @param[in] tid - Destination TID
 *  @param[in] transferOp - TransferOperation
 *  @param[in] fd - FileDescriptor
 *  @param[in] transferHandle - DataTransferHandle
 *  @param[in] requestSectionOffset - RequestedSectionOffset
 *  @param[in] requestSectionLength - RequestedSectionLengthBytes
 *  @param[out] completionCode - CompletionCode
 *  @param[out] transferFlag - TransferFlag
 *  @param[out] nextDataTransferHandle - NextDataTransferHandle
 *  @param[out] dataBuffer - Data buffer
 *  @param[out] dataIntegrityChecksum - DataIntegrityChecksum
 */
exec::task<int> sendMultipartReceive(
    TerminusManager& terminusManager, const pldm_tid_t& tid,
    const TransferOp& transferOp, const TransferCtx& fd,
    const TransferHandle& transferHandle,
    const SectionOffset& requestSectionOffset,
    const SectionOffset& requestSectionLength, uint8_t& completionCode,
    TransferFlag& transferFlag, TransferHandle& nextDataTransferHandle,
    std::vector<uint8_t>& dataBuffer, Checksum& dataIntegrityChecksum);

} // namespace file_cmds

} // namespace platform_mc

} // namespace pldm
