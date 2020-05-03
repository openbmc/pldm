#pragma once

#include "libpldm/base.h"
#include "libpldm/fru.h"
#include "libpldm/pldm_types.h"
#include "utils.hpp"

#include <string>

namespace pldm
{

using Interface = std::string;
using Property = std::string;
using Value = std::variant<bool, int64_t, std::string, std::vector<uint8_t>>;
using FruProperties = std::map<Property, Value>;
using FruProps = std::map<Interface, FruProperties>;

/** @class PLDMFRUParser
 *
 *  @brief Parses the PLDM FRU record and parse the fru record from the FRU
 * feild and map them to corresponding D-Bus properties
 */
class PLDMFRUParser
{
  public:
    PLDMFRUParser() = delete;
    PLDMFRUParser(PLDMFRUParser&) = delete;
    PLDMFRUParser(PLDMFRUParser&&) = delete;
    PLDMFRUParser& operator=(const PLDMFRUParser&) = delete;
    PLDMFRUParser& operator=(PLDMFRUParser&&) = delete;
    virtual ~PLDMFRUParser() = delete;

    FruProps fruProps(const uint8_t* fruRecord);

  private:
    const std::string assetInterface =
        "xyz.openbmc_project.Inventory.Decorator.Asset";

    void parseFruFeildData(const pldm_fru_record_tlv* fruRecordDataPtr,
                           uint8_t fruFeildNumber,
                           FruProperties* fruProperties);
};

} // namespace pldm