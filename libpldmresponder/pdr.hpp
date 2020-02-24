#pragma once

#include "effecters.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "utils.hpp"

#include <stdint.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <xyz/openbmc_project/Common/error.hpp>

#include "libpldm/pdr.h"
#include "libpldm/platform.h"

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
namespace fs = std::filesystem;

using namespace pldm::responder::pdr_utils;

namespace pldm
{

namespace responder
{

namespace pdr
{
/** @brief Parse PDR JSONs and build PDR repository
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *  @param[in] repo - instance of concrete implementation of Repo
 */
void generate(const std::string& dir, Repo& repo);

/** @brief Build (if not built already) and retrieve PDR
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *
 *  @return Repo& - Reference to instance of pdr::Repo
 */
Repo& getRepo(const std::string& dir);

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
 *  @param[in] pdrRepo - pdr::Repo
 *  @param[in] recordHandle - The recordHandle value for the PDR to be
 * retrieved.
 *  @param[out] pdrEntry - PDR entry structure reference
 *
 *  @return pldm_pdr_record - Instance of pdr::Repo
 */
const pldm_pdr_record* getRecordByHandle(Repo& pdrRepo,
                                         RecordHandle recordHandle,
                                         PdrEntry& pdrEntry);

} // namespace pdr
} // namespace responder
} // namespace pldm
