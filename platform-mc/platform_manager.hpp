#pragma once

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "terminus.hpp"
#include "terminus_manager.hpp"

namespace pldm
{

namespace platform_mc
{

/**
 * @brief PlatformManager
 *
 * PlatformManager class manages Terminus for fetching PDRs and initializing
 * sensors.
 */
class PlatformManager
{
  public:
    PlatformManager() = delete;
    PlatformManager(const PlatformManager&) = delete;
    PlatformManager(PlatformManager&&) = delete;
    PlatformManager& operator=(const PlatformManager&) = delete;
    PlatformManager& operator=(PlatformManager&&) = delete;
    ~PlatformManager() = default;

    explicit PlatformManager(
        TerminusManager& terminusManager,
        std::map<tid_t, std::shared_ptr<Terminus>>& termini) :
        terminusManager(terminusManager),
        termini(termini)
    {}

    /** @brief Initialize terminus which supports PLDM Type 2
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine initTerminus();

  private:
    /** @brief Fetch all PDRs from terminus.
     *
     *  @param[in] terminus - The terminus object to store fetched PDRs
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getPDRs(std::shared_ptr<Terminus> terminus);

    /** @brief Fetch PDR from terminus
     *
     *  @param[in] tid - Destination TID
     *  @param[in] recordHndl - Record handle
     *  @param[in] dataTransferHndl - Data transfer handle
     *  @param[in] transferOpFlag - Transfer Operation Flag
     *  @param[in] requstCnt - Request Count of data
     *  @param[in] recordChgNum - Record change number
     *  @param[out] nextRecordHndl - Next record handle
     *  @param[out] nextDataTransferHndl - Next data transfer handle
     *  @param[out] transferFlag - Transfer flag
     *  @param[out] responseCnt - Response count of record data
     *  @param[out] recordData - Returned record data
     *  @param[out] transferCrc - CRC value when record data is last part of PDR
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine
        getPDR(const tid_t tid, const uint32_t recordHndl,
               const uint32_t dataTransferHndl, const uint8_t transferOpFlag,
               const uint16_t requestCnt, const uint16_t recordChgNum,
               uint32_t& nextRecordHndl, uint32_t& nextDataTransferHndl,
               uint8_t& transferFlag, uint16_t& responseCnt,
               std::vector<uint8_t>& recordData, uint8_t& transferCrc);

    /** @brief get PDR repository information.
     *
     *  @param[in] tid - Destination TID
     *  @param[out] repositoryState - the state of repository
     *  @param[out] recordCount - number of records
     *  @param[out] repositorySize - repository size
     *  @param[out] largestRecordSize - largest record size
     * *
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getPDRRepositoryInfo(const tid_t tid,
                                              uint8_t& repositoryState,
                                              uint32_t& recordCount,
                                              uint32_t& repositorySize,
                                              uint32_t& largestRecordSize);

    /** @brief reference of terminusManager */
    TerminusManager& terminusManager;

    /** @brief Managed termini list */
    std::map<tid_t, std::shared_ptr<Terminus>>& termini;
};
} // namespace platform_mc
} // namespace pldm
