#pragma once

#include "config.h"

#include "handler.hpp"
#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/utils.hpp"

#include <stdint.h>

#include <map>

#include "libpldm/platform.h"
#include "libpldm/states.h"

namespace pldm
{
namespace responder
{
namespace platform_9
{

template <class DBusInterface>
int setTimedPowerOnValue(const DBusInterface& dBusIntf,
                         const std::string& objPath,
                         const std::string& dbusInterface,
                         uint8_t* effecterValue)
{
    // Update Dbus interfaces
    auto dbusProp = "PowerOnTime";
    uint32_t timeNow = time(0);
    uint32_t timeValue = *(reinterpret_cast<uint32_t*>(&effecterValue[0]));
    if (timeValue < timeNow)
    {
        std::cerr << "Error setting TPO time, effecterValue=" << timeValue
                  << "\n";
        return PLDM_ERROR;
    }
    std::variant<uint32_t> value{timeValue};
    try
    {
        dBusIntf.setDbusProperty(objPath.c_str(), dbusProp,
                                 dbusInterface.c_str(), value);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error setting TPO property, ERROR=" << e.what()
                  << " PROPERTY=" << dbusProp << " INTERFACE=" << dbusInterface
                  << " PATH=" << objPath.c_str() << "\n";
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

/** @brief Function to set the effecter value requested by pldm requester
 *  @param[in] dBusIntf - The interface object
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
                                   const uint8_t effecterId,
                                   const uint8_t effecterDataSize,
                                   uint8_t* effecterValue,
                                   size_t effecterValueLength)
{
    constexpr auto uint32_length = 4;
    using PropertyMap = std::map<uint16_t, std::string>;
    static const PropertyMap setNumToDbusProp = {
        {PLDM_TIMED_POWER_ON_STATE,
         "xyz.openbmc_project.Control.TimedPowerOn"}};
    using namespace pldm::responder::pdr;
    using namespace pldm::responder::effecter::dbus_mapping;

    // Get the pdr structure of pldm_numeric_effecter_valuer_pdr according
    // to the effecterId
    pldm_numeric_effecter_valuer_pdr* pdr = nullptr;
    uint32_t recordHndl{};
    Repo& pdrRepo = get(PDR_JSONS_DIR);
    pdr::Entry pdrEntry{};

    while (!pdr)
    {
        pdrEntry = pdrRepo.at(recordHndl);
        pldm_pdr_hdr* header = reinterpret_cast<pldm_pdr_hdr*>(pdrEntry.data());
        if (header->type != PLDM_NUMERIC_EFFECTER_PDR)
        {
            recordHndl = pdrRepo.getNextRecordHandle(recordHndl);
            if (recordHndl)
            {
                continue;
            }
            return PLDM_PLATFORM_INVALID_EFFECTER_ID;
        }
        pdr = reinterpret_cast<pldm_numeric_effecter_valuer_pdr*>(
            pdrEntry.data());
        recordHndl = pdr->hdr.record_handle;
        if (pdr->effecter_id == effecterId)
        {
            if (pdr->effecter_data_size != effecterDataSize ||
                effecterValueLength != uint32_length)
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

    // Get the value of effecterValue according to the value of
    // effecterDataSize
    std::map<uint16_t, std::function<int(const std::string& objPath,
                                         const std::string& dbusInterface,
                                         uint8_t* effecterValue)>>
        effecterToDbusEntries = {
            {PLDM_TIMED_POWER_ON_STATE,
             [&](const std::string& objPath, const std::string& dbusInterface,
                 uint8_t* effecterValue) {
                 return setTimedPowerOnValue(dBusIntf, objPath, dbusInterface,
                                             effecterValue);
             }}};

    auto iter = effecterToDbusEntries.find(effecterId);
    if (iter == effecterToDbusEntries.end())
    {
        std::cerr << "Did not find the state set for the"
                  << " effecterToDbusEntries, EFFECTER_ID=" << effecterId
                  << "\n";
        return PLDM_PLATFORM_INVALID_STATE_VALUE;
    }
    auto paths = get(effecterId);
    auto dbusInterface = setNumToDbusProp.at(effecterId);

    auto rc = iter->second(paths[0], dbusInterface, effecterValue);
    return rc;
}

} // namespace platform_9
} // namespace responder
} // namespace pldm
