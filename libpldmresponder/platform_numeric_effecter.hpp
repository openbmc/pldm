#pragma once

#include "config.h"

#include "handler.hpp"
#include "libpldmresponder/pdr.hpp"
#include "utils.hpp"

#include <stdint.h>

#include <map>

#include "libpldm/platform.h"
#include "libpldm/states.h"

using namespace pldm::responder::pdr;

namespace pldm
{
namespace responder
{
namespace platformNumericEffecter
{

using namespace pldm::responder::effecter::dbus_mapping;

template <typename T>
T getTimedPowerOnValue(const pldm_numeric_effecter_value_pdr* pdr,
                       uint8_t* effecterValue)
{
    uint64_t powerOnTime = *(reinterpret_cast<uint64_t*>(&effecterValue[0]));
    if (pdr->base_unit == 21 &&
        (pdr->unit_modifier == 0 || pdr->unit_modifier == 3))
    {
        powerOnTime = powerOnTime * pow(10, pdr->unit_modifier);
    }

    std::variant<uint64_t> value{powerOnTime};

    return value;
}

/** @brief Function to set the effecter value requested by pldm requester
 *  @param[in] dBusIntf - The interface object
 *  @param[in] pdrRepo - the interface API to the PDR repository
 *  @param[in] effecterId - Effecter ID sent by the requester to act on
 *  @param[in] effecterDataSize - The bit width and format of the setting
 * 				value for the effecter
 *  @param[in] effecter_value - The setting value of numeric effecter being
 * 				requested.
 *  @param[in] effecterValueLength - The setting value length of numeric
 *              effecter being requested.
 *  @return - Success or failure in setting the states. Returns failure in
 * terms of PLDM completion codes if atleast one state fails to be set
 */
template <class DBusInterface>
int setNumericEffecterValueHandler(const DBusInterface& dBusIntf,
                                   const Repo& pdrRepo,
                                   const uint16_t effecterId,
                                   const uint8_t effecterDataSize,
                                   uint8_t* effecterValue,
                                   size_t effecterValueLength)
{
    constexpr auto effecter_length = 4;
    using namespace pldm::responder::pdr;
    using namespace pldm::responder::effecter::dbus_mapping;

    if (pdrRepo.empty())
    {
        std::cerr << "PDR repo is empty." << std::endl;
        return PLDM_ERROR;
    }

    // Get the pdr structure of pldm_numeric_effecter_value_pdr according
    // to the effecterId
    pldm_numeric_effecter_value_pdr* pdr = nullptr;
    uint32_t recordHndl = 1;
    pdr::Entry pdrEntry{};

    while (!pdr)
    {
        pdrEntry = pdrRepo.at(recordHndl);
        pdr =
            reinterpret_cast<pldm_numeric_effecter_value_pdr*>(pdrEntry.data());
        if (pdr->effecter_id == effecterId)
        {
            if (pdr->effecter_data_size != effecterDataSize ||
                effecterValueLength != effecter_length)
            {
                std::cerr << "The requester sent wrong effecter"
                          << " data size for the effecter, EFFECTER_ID="
                          << effecterId
                          << "EFFECTER_DATA_SIZE=" << effecterDataSize << "\n";
                return PLDM_ERROR_INVALID_DATA;
            }
            break;
        }
        recordHndl = pdrRepo.getNextRecordHandle(recordHndl);
        if (!recordHndl)
        {
            return PLDM_PLATFORM_INVALID_EFFECTER_ID;
        }
        pdr = nullptr;
    }

    auto dbusObj = get(effecterId);
    auto updateDbusProperty = [&dBusIntf, &dbusObj](const auto& value) -> int {
        try
        {
            dBusIntf.setDbusProperty(dbusObj[0].objectPath.c_str(),
                                     dbusObj[0].propertyName.c_str(),
                                     dbusObj[0].interface.c_str(), value);
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {
            std::cerr << "Error setting TPO property, ERROR=" << e.what()
                      << " PROPERTY=" << dbusObj[0].propertyName.c_str()
                      << " INTERFACE=" << dbusObj[0].interface.c_str()
                      << " PATH=" << dbusObj[0].objectPath.c_str() << "\n";
            return PLDM_ERROR;
        }

        return PLDM_SUCCESS;
    };

    int rc = PLDM_SUCCESS;
    switch (effecterId)
    {
        case PLDM_TIMED_POWER_ON_STATE:
        {
            auto value = getTimedPowerOnValue<std::variant<uint64_t>>(
                pdr, effecterValue);
            rc = updateDbusProperty(value);
            break;
        }
        default:
        {
            std::cerr << "Did not find the state set for the"
                      << " effecterToDbusEntries, EFFECTER_ID=" << effecterId
                      << "\n";
            rc = PLDM_PLATFORM_INVALID_STATE_VALUE;
            break;
        }
    }

    return rc;
}

} // namespace platformNumericEffecter
} // namespace responder
} // namespace pldm
