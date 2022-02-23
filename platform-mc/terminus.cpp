#include "terminus.hpp"

#include "platform.h"

#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

void Terminus::fetchPDRs()
{
    deferredFetchPDREvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn((&Terminus::processFetchPDREvent)), this,
                         0, std::placeholders::_1));
}

void Terminus::processFetchPDREvent(
    uint32_t nextRecordHandle, sdeventplus::source::EventBase& /* source */)
{
    deferredFetchPDREvent.reset();
    this->sendGetPDR(nextRecordHandle, 0, PLDM_GET_FIRSTPART, UINT16_MAX, 0);
}

void Terminus::sendGetPDR(uint32_t record_hndl, uint32_t data_transfer_hndl,
                          uint8_t op_flag, uint16_t request_cnt,
                          uint16_t record_chg_num)
{
    auto instanceId = requester.getInstanceId(eid);
    Request requestMsg(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES);

    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_pdr_req(instanceId, record_hndl, data_transfer_hndl,
                                 op_flag, request_cnt, record_chg_num, request,
                                 PLDM_GET_PDR_REQ_BYTES);

    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "Failed to encode_get_pdr_req, EID=" << unsigned(eid)
                  << ", RC=" << rc << std::endl;
        return manager->initTerminus();
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_PLATFORM, PLDM_GET_PDR, std::move(requestMsg),
        std::move(std::bind_front(&Terminus::handleRespGetPDR, this)));
    if (rc)
    {
        std::cerr << "Failed to send GetPDR request, EID=" << unsigned(eid)
                  << ", RC=" << rc << std::endl;
        return manager->initTerminus();
    }
}

void Terminus::handleRespGetPDR(mctp_eid_t _eid, const pldm_msg* response,
                                size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        std::cerr << "No response received for getPDR, EID=" << unsigned(_eid)
                  << std::endl;
        return manager->initTerminus();
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t next_record_hndl;
    uint32_t next_data_transfer_hndl;
    uint8_t transfer_flag;
    uint16_t resp_cnt;
    uint8_t transfer_crc;

    auto rc = decode_get_pdr_resp(response, respMsgLen, &completionCode,
                                  &next_record_hndl, &next_data_transfer_hndl,
                                  &transfer_flag, &resp_cnt, nullptr, 0,
                                  &transfer_crc);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to decode_get_pdr_resp, EID=" << unsigned(_eid)
                  << ", RC=" << rc << std::endl;
        return manager->initTerminus();
    }

    std::vector<uint8_t> pdr(resp_cnt, 0);
    rc = decode_get_pdr_resp(response, respMsgLen, &completionCode,
                             &next_record_hndl, &next_data_transfer_hndl,
                             &transfer_flag, &resp_cnt, pdr.data(), resp_cnt,
                             &transfer_crc);
    if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
    {
        std::cerr << "Failed to decode_get_pdr_resp, EID=" << unsigned(_eid)
                  << ", RC =" << rc << ", CC=" << unsigned(completionCode)
                  << std::endl;
        return manager->initTerminus();
    }

    auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(pdr.data());
    if (pdrHdr->type == PLDM_NUMERIC_SENSOR_PDR)
    {
        numericSensorPDRs.emplace_back(pdr);
    }
    else if (pdrHdr->type == PLDM_ENTITY_AUXILIARY_NAMES_PDR)
    {
        entityAuxiliaryNamePDRs.emplace_back(pdr);
    }
    else
    {
        std::cerr << "found a pdr which is not support yet" << std::endl;
    }

    if (next_record_hndl)
    {
        deferredFetchPDREvent = std::make_unique<sdeventplus::source::Defer>(
            event, std::bind(std::mem_fn((&Terminus::processFetchPDREvent)),
                             this, next_record_hndl, std::placeholders::_1));
    }
    else
    {
        inventoryPath =
            "/xyz/openbmc_project/inventory/Item/Board/PLDM_Device_" +
            std::to_string(tid);
        inventoryItemBoardInft = std::make_shared<InventoryItemBoardIntf>(
            utils::DBusHandler::getBus(), inventoryPath.c_str());
        parsePDRs();
        return manager->initTerminus();
    }
}

void Terminus::parsePDRs()
{
    parseNumericSensorPDRs();
}

void Terminus::parseNumericSensorPDRs()
{
    for (const auto& pdr : numericSensorPDRs)
    {
        const auto& [terminusHandle, sensorInfo] = parseNumericPDR(pdr);
        addNumericSensor(sensorInfo);
    }
}

std::tuple<TerminusHandle, NumericSensorInfo>
    Terminus::parseNumericPDR(const std::vector<uint8_t>& pdrData)
{
    auto pdr = reinterpret_cast<const struct pldm_numeric_sensor_value_pdr*>(
        pdrData.data());
    NumericSensorInfo sensorInfo{};
    const uint8_t* ptr = &pdr->hysteresis.value_u8;

    sensorInfo.sensorId = pdr->sensor_id;
    sensorInfo.unitModifier = pdr->unit_modifier;
    sensorInfo.sensorDataSize = pdr->sensor_data_size;
    sensorInfo.resolution = pdr->resolution;
    sensorInfo.offset = pdr->offset;
    sensorInfo.baseUnit = pdr->base_unit;

    switch (pdr->sensor_data_size)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
            ptr += sizeof(pdr->hysteresis.value_u8);
            ptr += sizeof(pdr->supported_thresholds);
            ptr += sizeof(pdr->threshold_and_hysteresis_volatility);
            ptr += sizeof(pdr->state_transition_interval);
            ptr += sizeof(pdr->update_interval);
            sensorInfo.max = static_cast<uint8_t>(*ptr);
            ptr += sizeof(pdr->max_readable.value_u8);
            sensorInfo.min = static_cast<uint8_t>(*ptr);
            ptr += sizeof(pdr->min_readable.value_u8);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            ptr += sizeof(pdr->hysteresis.value_s8);
            ptr += sizeof(pdr->supported_thresholds);
            ptr += sizeof(pdr->threshold_and_hysteresis_volatility);
            ptr += sizeof(pdr->state_transition_interval);
            ptr += sizeof(pdr->update_interval);
            sensorInfo.max = static_cast<int8_t>(*ptr);
            ptr += sizeof(pdr->max_readable.value_s8);
            sensorInfo.min = static_cast<int8_t>(*ptr);
            ptr += sizeof(pdr->min_readable.value_s8);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
            ptr += sizeof(pdr->hysteresis.value_u16);
            ptr += sizeof(pdr->supported_thresholds);
            ptr += sizeof(pdr->threshold_and_hysteresis_volatility);
            ptr += sizeof(pdr->state_transition_interval);
            ptr += sizeof(pdr->update_interval);
            sensorInfo.max = static_cast<uint16_t>(*ptr);
            ptr += sizeof(pdr->max_readable.value_u16);
            sensorInfo.min = static_cast<uint16_t>(*ptr);
            ptr += sizeof(pdr->min_readable.value_u16);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            ptr += sizeof(pdr->hysteresis.value_s16);
            ptr += sizeof(pdr->supported_thresholds);
            ptr += sizeof(pdr->threshold_and_hysteresis_volatility);
            ptr += sizeof(pdr->state_transition_interval);
            ptr += sizeof(pdr->update_interval);
            sensorInfo.max = static_cast<int16_t>(*ptr);
            ptr += sizeof(pdr->max_readable.value_s16);
            sensorInfo.min = static_cast<int16_t>(*ptr);
            ptr += sizeof(pdr->min_readable.value_s16);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
            ptr += sizeof(pdr->hysteresis.value_u32);
            ptr += sizeof(pdr->supported_thresholds);
            ptr += sizeof(pdr->threshold_and_hysteresis_volatility);
            ptr += sizeof(pdr->state_transition_interval);
            ptr += sizeof(pdr->update_interval);
            sensorInfo.max = static_cast<uint32_t>(*ptr);
            ptr += sizeof(pdr->max_readable.value_u32);
            sensorInfo.min = static_cast<uint32_t>(*ptr);
            ptr += sizeof(pdr->min_readable.value_u32);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            ptr += sizeof(pdr->hysteresis.value_s32);
            ptr += sizeof(pdr->supported_thresholds);
            ptr += sizeof(pdr->threshold_and_hysteresis_volatility);
            ptr += sizeof(pdr->state_transition_interval);
            ptr += sizeof(pdr->update_interval);
            sensorInfo.max = static_cast<int32_t>(*ptr);
            ptr += sizeof(pdr->max_readable.value_s32);
            sensorInfo.min = static_cast<int32_t>(*ptr);
            ptr += sizeof(pdr->min_readable.value_s32);
            break;
        default:
            break;
    }

    uint8_t range_field_format = *ptr;
    ptr += sizeof(pdr->range_field_format);
    ptr += sizeof(pdr->range_field_support);

    switch (range_field_format)
    {
        case PLDM_RANGE_FIELD_FORMAT_UINT8:
            ptr += sizeof(pdr->nominal_value.value_u8);
            ptr += sizeof(pdr->normal_max.value_u8);
            ptr += sizeof(pdr->normal_min.value_u8);
            ptr += sizeof(pdr->warning_high.value_u8);
            ptr += sizeof(pdr->warning_low.value_u8);
            ptr += sizeof(pdr->critical_high.value_u8);
            ptr += sizeof(pdr->critical_low.value_u8);
            ptr += sizeof(pdr->fatal_high.value_u8);
            ptr += sizeof(pdr->fatal_low.value_u8);
            break;
        case PLDM_RANGE_FIELD_FORMAT_SINT8:
            ptr += sizeof(pdr->nominal_value.value_s8);
            ptr += sizeof(pdr->normal_max.value_s8);
            ptr += sizeof(pdr->normal_min.value_s8);
            ptr += sizeof(pdr->warning_high.value_s8);
            ptr += sizeof(pdr->warning_low.value_s8);
            ptr += sizeof(pdr->critical_high.value_s8);
            ptr += sizeof(pdr->critical_low.value_s8);
            ptr += sizeof(pdr->fatal_high.value_s8);
            ptr += sizeof(pdr->fatal_low.value_s8);
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT16:
            ptr += sizeof(pdr->nominal_value.value_u16);
            ptr += sizeof(pdr->normal_max.value_u16);
            ptr += sizeof(pdr->normal_min.value_u16);
            ptr += sizeof(pdr->warning_high.value_u16);
            ptr += sizeof(pdr->warning_low.value_u16);
            ptr += sizeof(pdr->critical_high.value_u16);
            ptr += sizeof(pdr->critical_low.value_u16);
            ptr += sizeof(pdr->fatal_high.value_u16);
            ptr += sizeof(pdr->fatal_low.value_u16);
            break;
        case PLDM_RANGE_FIELD_FORMAT_SINT16:
            ptr += sizeof(pdr->nominal_value.value_s16);
            ptr += sizeof(pdr->normal_max.value_s16);
            ptr += sizeof(pdr->normal_min.value_s16);
            ptr += sizeof(pdr->warning_high.value_s16);
            ptr += sizeof(pdr->warning_low.value_s16);
            ptr += sizeof(pdr->critical_high.value_s16);
            ptr += sizeof(pdr->critical_low.value_s16);
            ptr += sizeof(pdr->fatal_high.value_s16);
            ptr += sizeof(pdr->fatal_low.value_s16);
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT32:
            ptr += sizeof(pdr->nominal_value.value_u32);
            ptr += sizeof(pdr->normal_max.value_u32);
            ptr += sizeof(pdr->normal_min.value_u32);
            ptr += sizeof(pdr->warning_high.value_u32);
            ptr += sizeof(pdr->warning_low.value_u32);
            ptr += sizeof(pdr->critical_high.value_u32);
            ptr += sizeof(pdr->critical_low.value_u32);
            ptr += sizeof(pdr->fatal_high.value_u32);
            ptr += sizeof(pdr->fatal_low.value_u32);
            break;
        case PLDM_RANGE_FIELD_FORMAT_SINT32:
            ptr += sizeof(pdr->nominal_value.value_s32);
            ptr += sizeof(pdr->normal_max.value_s32);
            ptr += sizeof(pdr->normal_min.value_s32);
            ptr += sizeof(pdr->warning_high.value_s32);
            ptr += sizeof(pdr->warning_low.value_s32);
            ptr += sizeof(pdr->critical_high.value_s32);
            ptr += sizeof(pdr->critical_low.value_s32);
            ptr += sizeof(pdr->fatal_high.value_s32);
            ptr += sizeof(pdr->fatal_low.value_s32);
            break;
        case PLDM_RANGE_FIELD_FORMAT_REAL32:
            ptr += sizeof(pdr->nominal_value.value_f32);
            ptr += sizeof(pdr->normal_max.value_f32);
            ptr += sizeof(pdr->normal_min.value_f32);
            ptr += sizeof(pdr->warning_high.value_f32);
            ptr += sizeof(pdr->warning_low.value_f32);
            ptr += sizeof(pdr->critical_high.value_f32);
            ptr += sizeof(pdr->critical_low.value_f32);
            ptr += sizeof(pdr->fatal_high.value_f32);
            ptr += sizeof(pdr->fatal_low.value_f32);
            break;
        default:
            break;
    }
    return std::make_tuple(pdr->terminus_handle, std::move(sensorInfo));
}

void Terminus::addNumericSensor(const NumericSensorInfo& sensorInfo)
{
    auto sensor = std::make_shared<NumericSensor>(eid, tid, true, sensorInfo,
                                                  inventoryPath);
    numericSensors.emplace_back(std::move(sensor));
}

} // namespace platform_mc
} // namespace pldm
