#pragma once

#include "libpldmresponder/effecters.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "utils.hpp"

#include "libpldm/platform.h"

namespace fs = std::filesystem;

using namespace pldm::responder::pdr_utils;

using Type = uint8_t;
using generateHandler =
    std::function<void(const Json& json, RepoInterface& repo)>;

namespace pldm
{

namespace responder
{

namespace pdr
{
/** @brief Parse PDR JSONs and build PDR repository
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *  @param[in] repo - instance of the concrete implementation of RepoInterface
 */
void generate(const std::string& dir, RepoInterface& repo);

/** @brief Build (if not built already) and retrieve PDR
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *
 *  @return RepoInterface& - Reference to instance of pdr::RepoInterface
 */
RepoInterface& getRepo(const std::string& dir);

/** @brief Build (if not built already) and retrieve PDR by the PDR types
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *  @param[in] pdrType - the type of PDRs
 *
 *  @return Repo - Instance of pdr::Repo
 */
Repo getRepoByType(const std::string& dir, Type pdrType);

/** @brief Get the record of PDR by the record handle
 *
 *  @param[in] pdrRepo - pdr::RepoInterface
 *  @param[in] recordHandle - The recordHandle value for the PDR to be
 * retrieved.
 *  @param[out] pdrEntry - PDR entry structure reference
 *
 *  @return pldm_pdr_record - Instance of pdr::RepoInterface
 */
const pldm_pdr_record* getRecordByHandle(RepoInterface& pdrRepo,
                                         RecordHandle recordHandle,
                                         PdrEntry& pdrEntry);

} // namespace pdr
} // namespace responder
} // namespace pldm
