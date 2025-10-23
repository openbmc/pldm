#include "platform-mc/effecters/numeric/effecter.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"
#include "platform-mc/terminus_manager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cmath>
#include <filesystem>
#include <limits>
#include <regex>

namespace pldm
{
namespace platform_mc
{

void NumericEffecter::setEffecterUnit(uint8_t baseUnit)
{
    this->baseUnit = baseUnit;
    effecterNameSpace = "/xyz/openbmc_project/control/";
}

void NumericEffecter::registerInterface(
    std::unique_ptr<NumericEffecterDbusInterface> intf)
{
    if (intf)
    {
        interfaces.push_back(std::move(intf));
    }
}

double NumericEffecter::rawToUnit(double value)
{
    double convertedValue = value;
    convertedValue *= std::isnan(resolution) ? 1 : resolution;
    convertedValue += std::isnan(offset) ? 0 : offset;

    return convertedValue;
}

double NumericEffecter::unitToRaw(double value)
{
    if (resolution == 0)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    double convertedValue = value;
    convertedValue -= std::isnan(offset) ? 0 : offset;
    convertedValue /= std::isnan(resolution) ? 1 : resolution;

    return convertedValue;
}

double NumericEffecter::unitToBase(double value)
{
    double convertedValue = value;
    convertedValue *= std::pow(10, unitModifier);

    return convertedValue;
}

double NumericEffecter::baseToUnit(double value)
{
    double convertedValue = value;
    convertedValue *= std::pow(10, -unitModifier);

    return convertedValue;
}

NumericEffecter::NumericEffecter(
    const pldm_tid_t tid, std::shared_ptr<pldm_numeric_effecter_value_pdr> pdr,
    const std::string& effecterName, const std::string& associationPath,
    Terminus& terminus, TerminusManager& terminusManager) :
    name(effecterName), tid(tid), terminus(terminus),
    terminusManager(terminusManager), pdr(pdr)
{
    if (!pdr)
    {
        throw std::invalid_argument(std::format(
            "Invalid PDR passed for NumericEffecter: {}", effecterName));
    }

    effecterId = pdr->effecter_id;
    entityInfo = {ContainerID(pdr->container_id), EntityType(pdr->entity_type),
                  EntityInstance(pdr->entity_instance)};

    needsUpdate = true;

    dataSize = pdr->effecter_data_size;
    resolution = pdr->resolution;
    offset = pdr->offset;
    unitModifier = pdr->unit_modifier;

    setEffecterUnit(pdr->base_unit);

    path = std::filesystem::path(effecterNameSpace) / effecterName;
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
        {{"controlling", "controlled_by", associationPath.c_str()}});

    lg2::info("Created Numeric Effecter {NAME}.", "NAME", effecterName);
}

void NumericEffecter::updateValue(pldm_effecter_oper_state effecterOperState,
                                  double pendingValue, double presentValue)
{
    // Notify all registered interfaces of the value change
    for (auto& intf : interfaces)
    {
        if (!intf)
        {
            continue;
        }
        try
        {
            intf->handleValueChange(*this, effecterOperState,
                                    rawToBase(pendingValue),
                                    rawToBase(presentValue));
        }
        catch (const std::exception& e)
        {
            lg2::error("Exception in effecter interface for {NAME}: {ERROR}",
                       "NAME", name, "ERROR", e.what());
        }
    }
}

void NumericEffecter::handleErrGetNumericEffecterValue()
{
    // Notify all registered interfaces of the error
    for (auto& intf : interfaces)
    {
        if (!intf)
        {
            continue;
        }
        try
        {
            intf->handleError(*this);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Exception in effecter error interface for {NAME}: {ERROR}",
                "NAME", name, "ERROR", e.what());
        }
    }
}

exec::task<int> NumericEffecter::setNumericEffecterEnable(
    pldm_effecter_oper_state state)
{
    Request request(sizeof(pldm_msg_hdr) +
                    PLDM_PLATFORM_SET_NUMERIC_EFFECTER_ENABLE_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

    struct pldm_platform_set_numeric_effecter_enable_req req = {
        .effecter_id = static_cast<uint16_t>(effecterId),
        .effecter_operational_state = state};

    size_t reqLen = PLDM_PLATFORM_SET_NUMERIC_EFFECTER_ENABLE_REQ_BYTES;

    auto rc = encode_pldm_platform_set_numeric_effecter_enable_req(
        0, &req, requestMsg, &reqLen);
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
    rc = decode_pldm_platform_set_numeric_effecter_enable_resp(
        responseMsg, payloadLen, &completionCode);
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
    // Get the appropriate request size based on PLDM platform version
    // Default to v1.2.0 size (7 bytes)
    size_t maxReqSize = 7;

    auto version = terminus.getSupportedTypeVersion(PLDM_PLATFORM);
    if (version.has_value())
    {
        // v1.2.0 uses 7 bytes, v1.3.0 uses 11 bytes
        if (version->major == 1 && version->minor == 2)
        {
            maxReqSize = 7;
        }
        else if (version->major == 1 && version->minor == 3)
        {
            maxReqSize = 11;
        }
    }

    Request request(sizeof(pldm_msg_hdr) + maxReqSize);
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
        co_await getNumericEffecterValue();
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
