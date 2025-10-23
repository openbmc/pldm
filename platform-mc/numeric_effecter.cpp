/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "platform-mc/numeric_effecter.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"
#include "platform-mc/terminus_manager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <limits>
#include <regex>

namespace pldm
{
namespace platform_mc
{

static inline double getEffecterDataValue(uint8_t effecter_data_size,
                                          union_effecter_data_size value)
{
    double ret = std::numeric_limits<double>::quiet_NaN();
    switch (effecter_data_size)
    {
        case PLDM_EFFECTER_DATA_SIZE_UINT8:
            ret = value.value_u8;
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT8:
            ret = value.value_s8;
            break;
        case PLDM_EFFECTER_DATA_SIZE_UINT16:
            ret = value.value_u16;
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT16:
            ret = value.value_s16;
            break;
        case PLDM_EFFECTER_DATA_SIZE_UINT32:
            ret = value.value_u32;
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT32:
            ret = value.value_s32;
            break;
    }
    return ret;
}

void NumericEffecter::setEffecterUnit(uint8_t baseUnit)
{
    this->baseUnit = baseUnit;
    effecterNameSpace = "/xyz/openbmc_project/control/";
}

NumericEffecter::NumericEffecter(
    const pldm_tid_t tid, const bool effecterDisabled,
    std::shared_ptr<pldm_numeric_effecter_value_pdr> pdr,
    std::string& effecterName, std::string& associationPath,
    TerminusManager& terminusManager) :
    tid(tid), effecterName(effecterName), terminusManager(terminusManager)
{
    if (!pdr)
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    effecterId = pdr->effecter_id;
    entityInfo = {ContainerID(pdr->container_id), EntityType(pdr->entity_type),
                  EntityInstance(pdr->entity_instance)};

    std::string reverseAssociation = "all_controls";
    setEffecterUnit(pdr->base_unit);
    needsUpdate = true;

    path = effecterNameSpace + effecterName;
    path = std::regex_replace(path, std::regex("[^a-zA-Z0-9_/]+"), "_");

    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        associationDefinitionsIntf =
            std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Association interface for numeric effecter {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    associationDefinitionsIntf->associations(
        {{"chassis", reverseAssociation.c_str(), associationPath.c_str()}});

    double maxValue =
        getEffecterDataValue(pdr->effecter_data_size, pdr->max_settable);
    double minValue =
        getEffecterDataValue(pdr->effecter_data_size, pdr->min_settable);

    lg2::info(
        "Numeric Effecter {NAME} maxValue={MAX} minValue={MIN} baseUnit={BASEUNIT}",
        "NAME", effecterName, "MAX", maxValue, "MIN", minValue, "BASEUNIT",
        baseUnit);

    dataSize = pdr->effecter_data_size;
    resolution = pdr->resolution;
    offset = pdr->offset;
    unitModifier = pdr->unit_modifier;

    try
    {
        availabilityIntf =
            std::make_unique<AvailabilityIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Availability interface for numeric effecter {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    availabilityIntf->available(true);

    try
    {
        operationalStatusIntf =
            std::make_unique<OperationalStatusIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create OperationalStatus interface for numeric effecter {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    operationalStatusIntf->functional(!effecterDisabled);
}

void NumericEffecter::updateValue(pldm_effecter_oper_state effecterOperState,
                                  double pendingValue, double presentValue)
{
    bool available = true;
    bool functional = true;

    switch (effecterOperState)
    {
        case EFFECTER_OPER_STATE_ENABLED_UPDATEPENDING:
            available = true;
            functional = true;
            value = pendingValue;
            break;
        case EFFECTER_OPER_STATE_ENABLED_NOUPDATEPENDING:
            available = true;
            functional = true;
            value = presentValue;
            break;
        case EFFECTER_OPER_STATE_DISABLED:
            available = true;
            functional = false;
            break;
        case EFFECTER_OPER_STATE_INITIALIZING:
        case EFFECTER_OPER_STATE_UNAVAILABLE:
        case EFFECTER_OPER_STATE_STATUSUNKNOWN:
        case EFFECTER_OPER_STATE_FAILED:
        case EFFECTER_OPER_STATE_SHUTTINGDOWN:
        case EFFECTER_OPER_STATE_INTEST:
        default:
            available = false;
            functional = false;
            break;
    }
    if (availabilityIntf)
    {
        availabilityIntf->available(available);
    }

    if (operationalStatusIntf)
    {
        operationalStatusIntf->functional(functional);
    }
}

void NumericEffecter::handleErrGetNumericEffecterValue()
{
    if (availabilityIntf)
    {
        availabilityIntf->available(false);
    }

    if (operationalStatusIntf)
    {
        operationalStatusIntf->functional(false);
    }
}

exec::task<int> NumericEffecter::setNumericEffecterEnable(
    pldm_effecter_oper_state state)
{
    Request request(
        sizeof(pldm_msg_hdr) + PLDM_SET_NUMERIC_EFFECTER_ENABLE_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    struct pldm_set_numeric_effecter_enable_req req = {
        .effecter_id = effecterId, .effecter_operational_state = state};

    auto rc = encode_set_numeric_effecter_enable_req(
        0, &req, requestMsg, PLDM_SET_NUMERIC_EFFECTER_ENABLE_REQ_BYTES);
    if (rc)
    {
        lg2::error(
            "encode_set_numeric_effecter_enable_req failed, tid={TID}, rc={RC}.",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t payloadLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &payloadLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    rc = decode_cc_only_resp(responseMsg, payloadLen, &completionCode);
    if (rc)
    {
        lg2::error(
            "Failed to decode response of SetEffecterEnable, tid={TID}, rc={RC}.",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Failed to decode response of SetEffecterEnable, tid={TID}, rc={RC}, cc={CC}.",
            "TID", tid, "RC", rc, "CC", completionCode);
        co_await getNumericEffecterValue();
        co_return completionCode;
    }

    co_await getNumericEffecterValue();

    co_return completionCode;
}

exec::task<int> NumericEffecter::setNumericEffecterValue(double effecterValue)
{
    Request request(
        sizeof(pldm_msg_hdr) + PLDM_SET_NUMERIC_EFFECTER_VALUE_MAX_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    union_effecter_data_size effecterValueRaw;
    size_t payloadLength;
    switch (dataSize)
    {
        case PLDM_EFFECTER_DATA_SIZE_UINT8:
            effecterValueRaw.value_u8 = static_cast<uint8_t>(effecterValue);
            payloadLength = PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES;
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT8:
            effecterValueRaw.value_s8 = static_cast<int8_t>(effecterValue);
            payloadLength = PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES;
            break;
        case PLDM_EFFECTER_DATA_SIZE_UINT16:
            effecterValueRaw.value_u16 = static_cast<uint16_t>(effecterValue);
            payloadLength = PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 1;
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT16:
            effecterValueRaw.value_s16 = static_cast<int16_t>(effecterValue);
            payloadLength = PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 1;
            break;
        case PLDM_EFFECTER_DATA_SIZE_UINT32:
            effecterValueRaw.value_u32 = static_cast<uint32_t>(effecterValue);
            payloadLength = PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 3;
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT32:
        default:
            effecterValueRaw.value_s32 = static_cast<int32_t>(effecterValue);
            payloadLength = PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 3;
            break;
    }
    auto rc = encode_set_numeric_effecter_value_req(
        0, effecterId, dataSize, &effecterValueRaw.value_u8, requestMsg,
        payloadLength);
    if (rc)
    {
        lg2::error(
            "encode_set_numeric_effecter_value_req failed, tid={TID}, rc={RC}.",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t payloadLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &payloadLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    rc = decode_set_numeric_effecter_value_resp(responseMsg, payloadLen,
                                                &completionCode);
    if (rc)
    {
        lg2::error(
            "Failed to decode response of SetEffecterValue, tid={TID}, rc={RC}.",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Failed to decode response of SetEffecterValue, tid={TID}, cc={CC}.",
            "TID", tid, "CC", completionCode);
        co_await getNumericEffecterValue();
        co_return completionCode;
    }

    co_await getNumericEffecterValue();

    co_return completionCode;
}

exec::task<int> NumericEffecter::getNumericEffecterValue()
{
    Request request(
        sizeof(pldm_msg_hdr) + PLDM_GET_NUMERIC_EFFECTER_VALUE_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_get_numeric_effecter_value_req(0, effecterId, requestMsg);
    if (rc)
    {
        lg2::error(
            "encode_get_numeric_effecter_value_req failed, tid={TID}, rc={RC}.",
            "TID", tid, "RC", rc);
        co_return rc;
    }

    const pldm_msg* responseMsg = NULL;
    size_t payloadLen = 0;
    rc = co_await terminusManager.sendRecvPldmMsg(tid, request, &responseMsg,
                                                  &payloadLen);
    if (rc)
    {
        co_return rc;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint8_t effecterDataSize = PLDM_EFFECTER_DATA_SIZE_SINT32;
    uint8_t effecterOperationalState = 0;
    union_effecter_data_size pendingValueRaw;
    union_effecter_data_size presentValueRaw;
    rc = decode_get_numeric_effecter_value_resp(
        responseMsg, payloadLen, &completionCode, &effecterDataSize,
        &effecterOperationalState, reinterpret_cast<uint8_t*>(&pendingValueRaw),
        reinterpret_cast<uint8_t*>(&presentValueRaw));
    if (rc)
    {
        lg2::error(
            "Failed to decode response of getNumericEffecterValue, tid={TID}, effecterId={EFFECTERID}, rc={RC}.",
            "TID", tid, "EFFECTERID", effecterId, "RC", rc);
        handleErrGetNumericEffecterValue();
        co_return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        lg2::error(
            "Failed to decode response of getNumericEffecterValue, tid={TID}, effecterId={EFFECTERID}, cc={CC}.",
            "TID", tid, "EFFECTERID", effecterId, "CC", completionCode);
        handleErrGetNumericEffecterValue();
        co_return completionCode;
    }

    double pendingValue;
    double presentValue;
    switch (effecterDataSize)
    {
        case PLDM_EFFECTER_DATA_SIZE_UINT8:
            pendingValue = static_cast<double>(pendingValueRaw.value_u8);
            presentValue = static_cast<double>(presentValueRaw.value_u8);
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT8:
            pendingValue = static_cast<double>(pendingValueRaw.value_s8);
            presentValue = static_cast<double>(presentValueRaw.value_s8);
            break;
        case PLDM_EFFECTER_DATA_SIZE_UINT16:
            pendingValue = static_cast<double>(pendingValueRaw.value_u16);
            presentValue = static_cast<double>(presentValueRaw.value_u16);
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT16:
            pendingValue = static_cast<double>(pendingValueRaw.value_s16);
            presentValue = static_cast<double>(presentValueRaw.value_s16);
            break;
        case PLDM_EFFECTER_DATA_SIZE_UINT32:
            pendingValue = static_cast<double>(pendingValueRaw.value_u32);
            presentValue = static_cast<double>(presentValueRaw.value_u32);
            break;
        case PLDM_EFFECTER_DATA_SIZE_SINT32:
            pendingValue = static_cast<double>(pendingValueRaw.value_s32);
            presentValue = static_cast<double>(presentValueRaw.value_s32);
            break;
        default:
            pendingValue = std::numeric_limits<double>::quiet_NaN();
            presentValue = std::numeric_limits<double>::quiet_NaN();
            break;
    }

    updateValue(static_cast<pldm_effecter_oper_state>(effecterOperationalState),
                pendingValue, presentValue);
    co_return completionCode;
}

} // namespace platform_mc
} // namespace pldm
