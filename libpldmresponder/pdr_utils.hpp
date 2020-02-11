#pragma once

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

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

namespace fs = std::filesystem;

namespace pldm
{

namespace responder
{

namespace pdrUtils
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

} // namespace pdrUtils
} // namespace responder
} // namespace pldm
