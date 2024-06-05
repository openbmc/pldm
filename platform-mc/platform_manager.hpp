#pragma once

#include "libpldm/fru.h"
#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "dbus_impl_fru.hpp"
#include "terminus.hpp"
#include "terminus_manager.hpp"

#include <vector>

namespace pldm
{

namespace platform_mc
{

/**
 * @brief PlatformManager
 *
 * PlatformManager class manages the actions outlined in the platform spec.
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

    explicit PlatformManager(TerminusManager& terminusManager,
                             TerminiMapper& termini) :
        terminusManager(terminusManager),
        termini(termini)
    {
        this->frus.clear();
    }

    /** @brief Initialize terminus which supports PLDM Type 2
     *
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> initTerminus();

  private:
    /** @brief Fetch all PDRs from terminus.
     *
     *  @param[in] terminus - The terminus object to store fetched PDRs
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> getPDRs(std::shared_ptr<Terminus> terminus);

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
    exec::task<int>
        getPDR(const pldm_tid_t tid, const uint32_t recordHndl,
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
    exec::task<int> getPDRRepositoryInfo(const pldm_tid_t tid,
                                         uint8_t& repositoryState,
                                         uint32_t& recordCount,
                                         uint32_t& repositorySize,
                                         uint32_t& largestRecordSize);

    /** @brief Get FRU Record Tables from remote MCTP Endpoint
     *
     *  @param[in] tid - Destination TID
     *  @param[in] total - Total number of record in table
     */
    exec::task<int> getFRURecordTables(pldm_tid_t tid, const uint16_t& total);

    /** @brief Fetch FRU Record Data from terminus
     *
     *  @param[in] tid - Destination TID
     *  @param[in] dataTransferHndl - Data transfer handle
     *  @param[in] transferOpFlag - Transfer Operation Flag
     *  @param[out] nextDataTransferHndl - Next data transfer handle
     *  @param[out] transferFlag - Transfer flag
     *  @param[out] responseCnt - Response count of record data
     *  @param[out] recordData - Returned record data
     *
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> getFRURecordTable(pldm_tid_t tid,
                                      const uint32_t dataTransferHndl,
                                      const uint8_t transferOpFlag,
                                      uint32_t* nextDataTransferHndl,
                                      uint8_t* transferFlag,
                                      size_t* responseCnt,
                                      std::vector<uint8_t>& recordData);

    /** @brief Get FRU Record Table Metadata from remote MCTP Endpoint
     *
     *  @param[in] tid - Destination TID
     *  @param[out] total - Total number of record in table
     */
    exec::task<int> getFRURecordTableMetadata(pldm_tid_t tid, uint16_t* total);

    /** @brief Parse record data from FRU table
     *
     * @param[in] tid - Destination TID
     *  @param[in] fruData - pointer to FRU record table
     *  @param[in] fruLen - FRU table length
     */
    void createGernalFruDbus(pldm_tid_t tid, const uint8_t* fruData,
                             size_t& fruLen);

    /** reference of TerminusManager for sending PLDM request to terminus*/
    TerminusManager& terminusManager;

    /** @brief Managed termini list */
    TerminiMapper& termini;

    /** @brief Map of the object FRU */
    std::unordered_map<uint8_t, std::shared_ptr<pldm::dbus_api::FruReq>> frus;
};
} // namespace platform_mc
} // namespace pldm
