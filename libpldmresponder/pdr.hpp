#pragma once

#include "effecters.hpp"
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

namespace pldm
{

namespace responder
{

namespace pdr
{

struct PdrEntry
{
    uint8_t* data;
    uint32_t size;
    union
    {
        uint32_t current_handle;
        uint32_t next_handle;
    } handle;
};
using Type = uint8_t;
using Json = nlohmann::json;
using RecordHandle = uint32_t;

/** @brief Parse PDR JSON file and output Json object
 *
 *  @param[in] path - path of PDR JSON file
 *
 *  @return Json - Json object
 */
inline Json readJson(const std::string& path)
{
    std::ifstream jsonFile(path);
    if (!jsonFile.is_open())
    {
        std::cout << "Error opening PDR JSON file, PATH=" << path.c_str()
                  << std::endl;
        return {};
    }

    return Json::parse(jsonFile);
}

class Repo
{
  public:
    Repo()
    {
        repo = pldm_pdr_init();
    }

    ~Repo()
    {
        pldm_pdr_destroy(this->repo);
    }

    const pldm_pdr* getPdr();

    RecordHandle addRecord(PdrEntry pdrEntry);

    const pldm_pdr_record* getFirstRecord(PdrEntry& pdrEntry);

    const pldm_pdr_record* getNextRecord(const pldm_pdr_record* curr_record,
                                         PdrEntry& pdrEntry);

    uint32_t getRecordHandle(const pldm_pdr_record* record);

    uint32_t getRecordCount();

    bool empty();

  private:
    pldm_pdr* repo;
};

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
 *  @param[in] pdr_type - the different types of PDRs
 *
 *  @return Repo - Instance of pdr::Repo
 */
Repo getRepoByType(const std::string& dir, const Type pdr_type);

/** @brief Get the record of PDR by the record handle
 *
 *  @param[in] pdrRepo - pdr::Repo
 *  @param[in] record_handle - The recordHandle value for the PDR to be
 * retrieved.
 *  @param[out] pdrEntry - Pointer to PDR entry structure
 *
 *  @return pldm_pdr_record - Instance of pdr::Repo
 */
const pldm_pdr_record* getRecordByHandle(Repo& pdrRepo,
                                         const RecordHandle record_handle,
                                         PdrEntry& pdrEntry);

} // namespace pdr
} // namespace responder
} // namespace pldm
