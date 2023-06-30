#pragma once

#include "common/utils.hpp"
#include "libpldmresponder/pdr.hpp"
#include "pdr_utils.hpp"
#include "pldmd/handler.hpp"

#include <libpldm/platform.h>
#include <libpldm/states.h>
#include <math.h>
#include <stdint.h>

#include <phosphor-logging/lg2.hpp>

#include <map>
#include <optional>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{
namespace platform_numeric_effecter
{
/** @brief Function to get the effecter value by PDR factor coefficient, etc.
 *  @param[in] pdr - The structure of pldm_numeric_effecter_value_pdr.
 *  @param[in] effecterValue - effecter value.
 *  @param[in] propertyType - type of the D-Bus property.
 *
 *  @return - std::pair<int, std::optional<PropertyValue>> - rc:Success or
 *          failure, PropertyValue: The value to be set
 */
template <typename T>
std::pair<int, std::optional<pldm::utils::PropertyValue>>
    getEffecterRawValue(const pldm_numeric_effecter_value_pdr* pdr,
                        T& effecterValue, std::string propertyType)
{
    // X = Round [ (Y - B) / m ]
    // refer to DSP0248_1.2.0 27.8
    int rc = 0;
    pldm::utils::PropertyValue value;
    switch (pdr->effecter_data_size)
    {
        case PLDM_EFFECTER_DATA_SIZE_UINT8:
        {
            auto rawValue = static_cast<uint8_t>(
                round(effecterValue - pdr->offset) / pdr->resolution);
            if (pdr->min_settable.value_u8 < pdr->max_settable.value_u8 &&
                (rawValue < pdr->min_settable.value_u8 ||
                 rawValue > pdr->max_settable.value_u8))
            {
                rc = PLDM_ERROR_INVALID_DATA;
            }
            value = rawValue;
            if (propertyType == "uint64_t")
            {
                auto tempValue = std::get<uint8_t>(value);
                value = static_cast<uint64_t>(tempValue);
            }
            else if (propertyType == "uint32_t")
            {
                auto tempValue = std::get<uint8_t>(value);
                value = static_cast<uint32_t>(tempValue);
            }
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_SINT8:
        {
            auto rawValue = static_cast<int8_t>(
                round(effecterValue - pdr->offset) / pdr->resolution);
            if (pdr->min_settable.value_s8 < pdr->max_settable.value_s8 &&
                (rawValue < pdr->min_settable.value_s8 ||
                 rawValue > pdr->max_settable.value_s8))
            {
                rc = PLDM_ERROR_INVALID_DATA;
            }
            value = rawValue;
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_UINT16:
        {
            auto rawValue = static_cast<uint16_t>(
                round(effecterValue - pdr->offset) / pdr->resolution);
            if (pdr->min_settable.value_u16 < pdr->max_settable.value_u16 &&
                (rawValue < pdr->min_settable.value_u16 ||
                 rawValue > pdr->max_settable.value_u16))
            {
                rc = PLDM_ERROR_INVALID_DATA;
            }
            value = rawValue;
            if (propertyType == "uint64_t")
            {
                auto tempValue = std::get<uint16_t>(value);
                value = static_cast<uint64_t>(tempValue);
            }
            else if (propertyType == "uint32_t")
            {
                auto tempValue = std::get<uint16_t>(value);
                value = static_cast<uint32_t>(tempValue);
            }
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_SINT16:
        {
            auto rawValue = static_cast<int16_t>(
                round(effecterValue - pdr->offset) / pdr->resolution);
            if (pdr->min_settable.value_s16 < pdr->max_settable.value_s16 &&
                (rawValue < pdr->min_settable.value_s16 ||
                 rawValue > pdr->max_settable.value_s16))
            {
                rc = PLDM_ERROR_INVALID_DATA;
            }
            value = rawValue;
            if (propertyType == "uint64_t")
            {
                auto tempValue = std::get<int16_t>(value);
                value = static_cast<uint64_t>(tempValue);
            }
            else if (propertyType == "uint32_t")
            {
                auto tempValue = std::get<int16_t>(value);
                value = static_cast<uint32_t>(tempValue);
            }
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_UINT32:
        {
            auto rawValue = static_cast<uint32_t>(
                round(effecterValue - pdr->offset) / pdr->resolution);
            if (pdr->min_settable.value_u32 < pdr->max_settable.value_u32 &&
                (rawValue < pdr->min_settable.value_u32 ||
                 rawValue > pdr->max_settable.value_u32))
            {
                rc = PLDM_ERROR_INVALID_DATA;
            }
            value = rawValue;
            if (propertyType == "uint64_t")
            {
                auto tempValue = std::get<uint32_t>(value);
                value = static_cast<uint64_t>(tempValue);
            }
            else if (propertyType == "uint32_t")
            {
                auto tempValue = std::get<uint32_t>(value);
                value = static_cast<uint32_t>(tempValue);
            }
            break;
        }
        case PLDM_EFFECTER_DATA_SIZE_SINT32:
        {
            auto rawValue = static_cast<int32_t>(
                round(effecterValue - pdr->offset) / pdr->resolution);
            if (pdr->min_settable.value_s32 < pdr->max_settable.value_s32 &&
                (rawValue < pdr->min_settable.value_s32 ||
                 rawValue > pdr->max_settable.value_s32))
            {
                rc = PLDM_ERROR_INVALID_DATA;
            }
            value = rawValue;
            if (propertyType == "uint64_t")
            {
                auto tempValue = std::get<int32_t>(value);
                value = static_cast<uint64_t>(tempValue);
            }
            else if (propertyType == "uint32_t")
            {
                auto tempValue = std::get<int32_t>(value);
                value = static_cast<uint32_t>(tempValue);
            }
            break;
        }
    }

    return {rc, std::make_optional(std::move(value))};
}

/** @brief Function to convert the D-Bus value by PDR factor and effecter value.
 *  @param[in] pdr - The structure of pldm_numeric_effecter_value_pdr.
 *  @param[in] effecterDataSize - effecter value size.
 *  @param[in,out] effecterValue - effecter value.
 *  @param[in] propertyType - type of the D-Bus property.
 *
 *  @return std::pair<int, std::optional<PropertyValue>> - rc:Success or
 *          failure, PropertyValue: The value to be set
 */
std::pair<int, std::optional<pldm::utils::PropertyValue>>
    convertToDbusValue(const pldm_numeric_effecter_value_pdr* pdr,
                       uint8_t effecterDataSize, uint8_t* effecterValue,
                       std::string propertyType)
{
    if (effecterDataSize == PLDM_EFFECTER_DATA_SIZE_UINT8)
    {
        uint8_t currentValue = *(reinterpret_cast<uint8_t*>(&effecterValue[0]));
        return getEffecterRawValue<uint8_t>(pdr, currentValue, propertyType);
    }
    else if (effecterDataSize == PLDM_EFFECTER_DATA_SIZE_SINT8)
    {
        int8_t currentValue = *(reinterpret_cast<int8_t*>(&effecterValue[0]));
        return getEffecterRawValue<int8_t>(pdr, currentValue, propertyType);
    }
    else if (effecterDataSize == PLDM_EFFECTER_DATA_SIZE_UINT16)
    {
        uint16_t currentValue =
            *(reinterpret_cast<uint16_t*>(&effecterValue[0]));
        return getEffecterRawValue<uint16_t>(pdr, currentValue, propertyType);
    }
    else if (effecterDataSize == PLDM_EFFECTER_DATA_SIZE_SINT16)
    {
        int16_t currentValue = *(reinterpret_cast<int16_t*>(&effecterValue[0]));
        return getEffecterRawValue<int16_t>(pdr, currentValue, propertyType);
    }
    else if (effecterDataSize == PLDM_EFFECTER_DATA_SIZE_UINT32)
    {
        uint32_t currentValue =
            *(reinterpret_cast<uint32_t*>(&effecterValue[0]));
        return getEffecterRawValue<uint32_t>(pdr, currentValue, propertyType);
    }
    else if (effecterDataSize == PLDM_EFFECTER_DATA_SIZE_SINT32)
    {
        int32_t currentValue = *(reinterpret_cast<int32_t*>(&effecterValue[0]));
        return getEffecterRawValue<int32_t>(pdr, currentValue, propertyType);
    }
    else
    {
        error("Wrong field effecterDataSize...");
        return {PLDM_ERROR, {}};
    }
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
    if (!numericEffecterPdrRepo)
    {
        error("Failed to instantiate numeric effecter PDR repository");
        return PLDM_ERROR;
    }
    pldm::responder::pdr_utils::Repo numericEffecterPDRs(
        numericEffecterPdrRepo.get());
    pldm::responder::pdr::getRepoByType(handler.getRepo(), numericEffecterPDRs,
                                        PLDM_NUMERIC_EFFECTER_PDR);
    if (numericEffecterPDRs.empty())
    {
        error("The Numeric Effecter PDR repo is empty.");
        return PLDM_ERROR;
    }

    // Get the pdr structure of pldm_numeric_effecter_value_pdr according
    // to the effecterId
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
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

    if (effecterValueLength != effecterValueArrayLength)
    {
        error("effecter data size is incorrect.");
        return PLDM_ERROR_INVALID_DATA;
    }

    try
    {
        const auto& [dbusMappings,
                     dbusValMaps] = handler.getDbusObjMaps(effecterId);
        pldm::utils::DBusMapping dbusMapping{
            dbusMappings[0].objectPath, dbusMappings[0].interface,
            dbusMappings[0].propertyName, dbusMappings[0].propertyType};

        // convert to dbus effectervalue according to the factor
        auto [rc, dbusValue] = convertToDbusValue(
            pdr, effecterDataSize, effecterValue, dbusMappings[0].propertyType);
        if (rc != PLDM_SUCCESS)
        {
            return rc;
        }
        try
        {
            dBusIntf.setDbusProperty(dbusMapping, dbusValue.value());
        }
        catch (const std::exception& e)
        {
            error(
                "Error setting property, ERROR={ERR_EXCEP} PROPERTY={DBUS_PROP} INTERFACE={DBUS_INTF} PATH={DBUS_OBJ_PATH}",
                "ERR_EXCEP", e.what(), "DBUS_PROP", dbusMapping.propertyName,
                "DBUS_INTF", dbusMapping.interface, "DBUS_OBJ_PATH",
                dbusMapping.objectPath.c_str());
            return PLDM_ERROR;
        }
    }
    catch (const std::out_of_range& e)
    {
        error("Unknown effecter ID : {EFFECTER_ID} {ERR_EXCEP}", "EFFECTER_ID",
              effecterId, "ERR_EXCEP", e.what());
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace platform_numeric_effecter
} // namespace responder
} // namespace pldm
