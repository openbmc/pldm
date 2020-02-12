#pragma once

#include "libpldmresponder/pdr_utils.hpp"

#include <stdint.h>

#include <string>

#include "libpldm/pdr.h"

using namespace pldm::responder::pdr_utils;

namespace pldm
{

namespace responder
{

namespace pdr
{

/** @brief Build (if not built already) and retrieve PDR by the PDR types
 *
 *  @param[in] inRepo - pdr::RepoInterface
 *  @param[in] pdrType - the type of PDRs
 *
 *  @return Repo - Instance of pdr::Repo
 */
Repo getRepoByType(const Repo& inRepo, Type pdrType);

/** @brief Get the record of PDR by the record handle
 *
 *  @param[in] pdrRepo - pdr::RepoInterface
 *  @param[in] recordHandle - The recordHandle value for the PDR to be
 * retrieved.
 *  @param[out] pdrEntry - PDR entry structure reference
 *
 *  @return pldm_pdr_record - Instance of pdr::RepoInterface
 */
const pldm_pdr_record* getRecordByHandle(const RepoInterface& pdrRepo,
                                         RecordHandle recordHandle,
                                         PdrEntry& pdrEntry);

} // namespace pdr
} // namespace responder
} // namespace pldm