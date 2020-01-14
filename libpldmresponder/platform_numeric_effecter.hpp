#pragma once

#include "config.h"

#include "handler.hpp"
#include "libpldmresponder/pdr.hpp"
#include "pdr_utils.hpp"
#include "utils.hpp"

#include <math.h>
#include <stdint.h>

#include <map>
#include <optional>

#include "libpldm/platform.h"
#include "libpldm/states.h"

using namespace pldm::responder::pdr;
using namespace pldm::utils;

namespace pldm
{
namespace responder
{
namespace platform_numeric_effecter
{

/** @brief Function to get the effecter value by PDR factor coefficient, etc.
 *  @param[in] pdr - The structure of pldm_numeric_effecter_value_pdr.
 *  @tparam[in] minValue - The minimum legal setting value that the effecter
 *                         accepts.
 *  @tparam[in] maxValue - The maximum legal setting value that the effecter
 *                         accepts.
 *  @tparam[in,out] value - effecter value.
 *
 *  @return - Success or failure
 */
template <typename T>
int getEffecterValue(const pldm_numeric_effecter_value_pdr* pdr, T minValue,
                     T maxValue, T& value)
{

    // X = Round [ (Y - B) / m ]
    // refer to DSP0248_1.2.0 27.8
    value = static_cast<T>(round(value - pdr->offset) / pdr->resolution);

    if ((maxValue > minValue) && (value < minValue || value > maxValue))
    {
        return PLDM_ERROR_INVALID_DATA;
    }

    return PLDM_SUCCESS;
}

/** @brief Function to convert the D-Bus value by PDR factor and effecter value.
 *  @param[in] pdr - The structure of pldm_numeric_effecter_value_pdr.
 *  @param[in,out] effecterValue - effecter value.
 *
 *  @return std::pair<int, std::optional<PropertyValue>> - rc:Success or
 *          failure, PropertyValue: The value to be set
 */
std::pair<int, std::optional<PropertyValue>>
    convertToDbusValue(const pldm_numeric_effecter_value_pdr* pdr,
                       uint8_t* effecterValue)
{
    int rc = 0;
    PropertyValue value;
    switch (pdr->effecter_data_size)
    {
        case PLDM_EFFECTER_DATA_SIZE_UINT8:
        {
            uint8_t currentValue =
                *(reinterpret_cast<uint8_t*>(&effecterValue[0]));
            rc = getEffecterValue<uint8_t>(pdr, pdr->min_set_table.value_u8,
                                           pdr->max_set_table.value_u8,
                                           currentValue);
            value = static_cast<int8_t>(currentValue);
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_SINT8:
        {
            int8_t currentValue =
                *(reinterpret_cast<int8_t*>(&effecterValue[0]));
            rc = getEffecterValue<int8_t>(pdr, pdr->min_set_table.value_s8,
                                          pdr->max_set_table.value_s8,
                                          currentValue);
            value = static_cast<int8_t>(currentValue);
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_UINT16:
        {
            uint16_t currentValue =
                *(reinterpret_cast<uint16_t*>(&effecterValue[0]));
            rc = getEffecterValue<uint16_t>(pdr, pdr->min_set_table.value_u16,
                                            pdr->max_set_table.value_u16,
                                            currentValue);
            value = static_cast<uint16_t>(currentValue);
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_SINT16:
        {
            int16_t currentValue =
                *(reinterpret_cast<int16_t*>(&effecterValue[0]));
            rc = getEffecterValue<int16_t>(pdr, pdr->min_set_table.value_s16,
                                           pdr->max_set_table.value_s16,
                                           currentValue);
            value = static_cast<int16_t>(currentValue);
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_UINT32:
        {
            uint32_t currentValue =
                *(reinterpret_cast<uint32_t*>(&effecterValue[0]));
            rc = getEffecterValue<uint32_t>(pdr, pdr->min_set_table.value_u32,
                                            pdr->max_set_table.value_u32,
                                            currentValue);
            value = static_cast<uint32_t>(currentValue);
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_SINT32:
        {
            int32_t currentValue =
                *(reinterpret_cast<int32_t*>(&effecterValue[0]));
            rc = getEffecterValue<int32_t>(pdr, pdr->min_set_table.value_s32,
                                           pdr->max_set_table.value_s32,
                                           currentValue);
            value = static_cast<int32_t>(currentValue);
            break;
        }
        default:
            break;
    }

    return {rc, std::make_optional(std::move(value))};
}

/** @brief Function to set the effecter value requested by pldm requester
 *  @tparam[in] DBusInterface - DBus interface type
 *  @tparam[in] Handler - pldm::responder::platform::Handler
 *  @param[in] dBusIntf - The interface object of DBusInterface
 *  @param[in] handler - The interface object of
 *             pldm::responder::platform::Handler
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
template <class DBusInterface, class Handler>
int setNumericEffecterValueHandler(const DBusInterface& dBusIntf,
                                   Handler& handler, uint16_t effecterId,
                                   uint8_t effecterDataSize,
                                   uint8_t* effecterValue,
                                   size_t effecterValueLength)
{
    constexpr auto effecterValueArrayLength = 4;
    pldm_numeric_effecter_value_pdr* pdr = nullptr;

    std::unique_ptr<pldm_pdr, decltype(&pldm_pdr_destroy)>
        numericEffecterPdrRepo(pldm_pdr_init(), pldm_pdr_destroy);
    Repo numericEffecterPDRs(numericEffecterPdrRepo.get());
    getRepoByType(handler.getRepo(), numericEffecterPDRs,
                  PLDM_NUMERIC_EFFECTER_PDR);
    if (numericEffecterPDRs.empty())
    {
        std::cerr << "The Numeric Effecter PDR repo is empty." << std::endl;
        return PLDM_ERROR;
    }

    // Get the pdr structure of pldm_numeric_effecter_value_pdr according
    // to the effecterId
    PdrEntry pdrEntry{};
    auto pdrRecord = numericEffecterPDRs.getFirstRecord(pdrEntry);
    while (pdrRecord)
    {
        pdr = reinterpret_cast<pldm_numeric_effecter_value_pdr*>(pdrEntry.data);
        if (pdr->effecter_id != effecterId)
        {
            pdr = nullptr;
            pdrRecord = numericEffecterPDRs.getNextRecord(pdrRecord, pdrEntry);
            continue;
        }

        break;
    }

    if (!pdr)
    {
        return PLDM_PLATFORM_INVALID_EFFECTER_ID;
    }

    if (pdr->effecter_data_size != effecterDataSize ||
        effecterValueLength != effecterValueArrayLength)
    {
        std::cerr << "effecter data size is incorrect.\n";
        return PLDM_ERROR_INVALID_DATA;
    }

    // convert to dbus effectervalue according to the factor
    auto [rc, dbusValue] = convertToDbusValue(pdr, effecterValue);
    if (rc != PLDM_SUCCESS)
    {
        return rc;
    }

    const auto& [dbusMappings, dbusValMaps] =
        handler.getDbusObjMaps(effecterId);
    DBusMapping dbusMapping{
        dbusMappings[0].objectPath, dbusMappings[0].interface,
        dbusMappings[0].propertyName, dbusMappings[0].propertyType};
    try
    {
        dBusIntf.setDbusProperty(dbusMapping, dbusValue.value());
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error setting property, ERROR=" << e.what()
                  << " PROPERTY=" << dbusMapping.propertyName << " INTERFACE="
                  << dbusMapping.interface << " PATH=" << dbusMapping.objectPath
                  << "\n";
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace platform_numeric_effecter
} // namespace responder
} // namespace pldm
