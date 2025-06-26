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
 *  @param[in] reqPtr - pointer to the request struct
 *  @param[out] respPtr - pointer to the response struct
 *
 *  @return PLDM completion code
 */
exec::task<int> dfOpen(TerminusManager& terminusManager, pldm_tid_t tid,
                       const struct pldm_file_df_open_req* reqPtr,
                       struct pldm_file_df_open_resp* respPtr);

/** @brief Send DfClose command to file host
 *
 *  @param[in] terminusManager - TerminusManager object
 *  @param[in] tid - Destination TID
 *  @param[in] reqPtr - pointer to the request struct
 *
 *  @return PLDM completion code
 */
exec::task<int> dfClose(TerminusManager& terminusManager, pldm_tid_t tid,
                        const struct pldm_file_df_close_req* reqPtr);

/** @brief Send DfHeartbeat command to file host
 *
 *  @param[in] terminusManager - TerminusManager object
 *  @param[in] tid - Destination TID
 *  @param[in] reqPtr - pointer to the request struct
 *  @param[out] respPtr - pointer to the response struct
 *
 *  @return PLDM completion code
 */
exec::task<int> dfHeartbeat(TerminusManager& terminusManager, pldm_tid_t tid,
                            const struct pldm_file_df_heartbeat_req* reqPtr,
                            struct pldm_file_df_heartbeat_resp* respPtr);

/** @brief Send NegotiateTransferParameters command to file host
 *
 *  @param[in] terminusManager - TerminusManager object
 *  @param[in] tid - Destination TID
 *  @param[in] reqPtr - pointer to the request struct
 *  @param[out] respPtr - pointer to the response struct
 *
 *  @return PLDM completion code
 */
exec::task<int> negotiateTransferParameters(
    TerminusManager& terminusManager, pldm_tid_t tid,
    const struct pldm_base_negotiate_transfer_params_req* reqPtr,
    struct pldm_base_negotiate_transfer_params_resp* respPtr);

/** @brief Send MultipartReceive command to file host
 *
 *  @param[in] terminusManager - TerminusManager object
 *  @param[in] tid - Destination TID
 *  @param[in] reqPtr - pointer to the request struct
 *  @param[out] respPtr - pointer to the response struct
 *  @param[out] dataIntegrityChecksum - DataIntegrityChecksum
 *
 *  @return PLDM completion code
 */
exec::task<int> sendMultipartReceive(
    TerminusManager& terminusManager, pldm_tid_t tid,
    const struct pldm_base_multipart_receive_req* reqPtr,
    struct pldm_base_multipart_receive_resp* respPtr,
    CRC32Type& dataIntegrityChecksum);

} // namespace file_cmds

} // namespace platform_mc

} // namespace pldm
