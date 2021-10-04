#include "oem_ibm_handler.hpp"

#include "libpldm/entity.h"
#include "libpldm/requester/pldm.h"

#include "file_io_type_lid.hpp"
#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/pdr_utils.hpp"

using namespace pldm::pdr;
using namespace pldm::utils;

namespace pldm
{
namespace responder
{
namespace oem_ibm_platform
{
int pldm::responder::oem_ibm_platform::Handler::
    oemSetNumericEffecterValueHandler(
        uint16_t entityType, uint16_t entityInstance,
        uint16_t effecterSemanticId, uint8_t effecterDataSize,
        uint8_t* effecterValue, real32_t effecterOffset,
        real32_t effecterResolution, uint16_t effecterId)
{
    int rc = PLDM_SUCCESS;

    if (entityType == PLDM_ENTITY_PROC &&
        effecterSemanticId == PLDM_OEM_IBM_SBE_SEMANTIC_ID &&
        effecterDataSize == PLDM_EFFECTER_DATA_SIZE_UINT32)
    {
        uint32_t currentValue =
            *(reinterpret_cast<uint32_t*>(&effecterValue[0]));
        auto rawValue = static_cast<uint32_t>(
            round(currentValue - effecterOffset) / effecterResolution);
        pldm::utils::PropertyValue value;
        value = rawValue;

        for (auto& [key, value] : instanceMap)
        {
            if (key == effecterId)
            {
                entityInstance =
                    (value.dcmId * 2) + value.procId; // failingUintId
            }
        }
        rc = setNumericEffecter(entityInstance, value);
    }
    return rc;
}

int pldm::responder::oem_ibm_platform::Handler::
    getOemStateSensorReadingsHandler(
        EntityType entityType, EntityInstance entityInstance,
        StateSetId stateSetId, CompositeCount compSensorCnt,
        std::vector<get_sensor_state_field>& stateField)
{
    int rc = PLDM_SUCCESS;
    stateField.clear();

    for (size_t i = 0; i < compSensorCnt; i++)
    {
        uint8_t sensorOpState{};
        if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
            stateSetId == PLDM_OEM_IBM_BOOT_STATE)
        {
            sensorOpState = fetchBootSide(entityInstance, codeUpdate);
        }
        else
        {
            rc = PLDM_PLATFORM_INVALID_STATE_VALUE;
            break;
        }
        stateField.push_back({PLDM_SENSOR_ENABLED, PLDM_SENSOR_UNKNOWN,
                              PLDM_SENSOR_UNKNOWN, sensorOpState});
    }
    return rc;
}

int pldm::responder::oem_ibm_platform::Handler::
    oemSetStateEffecterStatesHandler(
        uint16_t entityType, uint16_t entityInstance, uint16_t stateSetId,
        uint8_t compEffecterCnt,
        std::vector<set_effecter_state_field>& stateField,
        uint16_t /*effecterId*/)
{
    int rc = PLDM_SUCCESS;

    for (uint8_t currState = 0; currState < compEffecterCnt; ++currState)
    {
        if (stateField[currState].set_request == PLDM_REQUEST_SET)
        {
            if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
                stateSetId == PLDM_OEM_IBM_BOOT_STATE)
            {
                rc = setBootSide(entityInstance, currState, stateField,
                                 codeUpdate);
            }
            else if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
                     stateSetId == PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
            {
                if (stateField[currState].effecter_state ==
                    uint8_t(CodeUpdateState::START))
                {
                    codeUpdate->setCodeUpdateProgress(true);
                    startUpdateEvent =
                        std::make_unique<sdeventplus::source::Defer>(
                            event,
                            std::bind(std::mem_fn(&oem_ibm_platform::Handler::
                                                      _processStartUpdate),
                                      this, std::placeholders::_1));
                }
                else if (stateField[currState].effecter_state ==
                         uint8_t(CodeUpdateState::END))
                {
                    rc = PLDM_SUCCESS;
                    assembleImageEvent = std::make_unique<
                        sdeventplus::source::Defer>(
                        event,
                        std::bind(
                            std::mem_fn(
                                &oem_ibm_platform::Handler::_processEndUpdate),
                            this, std::placeholders::_1));

                    // sendCodeUpdateEvent(effecterId, END, START);
                }
                else if (stateField[currState].effecter_state ==
                         uint8_t(CodeUpdateState::ABORT))
                {
                    codeUpdate->setCodeUpdateProgress(false);
                    codeUpdate->clearDirPath(LID_STAGING_DIR);
                    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
                    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                                         uint8_t(CodeUpdateState::ABORT),
                                         uint8_t(CodeUpdateState::START));
                    // sendCodeUpdateEvent(effecterId, ABORT, END);
                }
                else if (stateField[currState].effecter_state ==
                         uint8_t(CodeUpdateState::ACCEPT))
                {
                    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
                    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                                         uint8_t(CodeUpdateState::ACCEPT),
                                         uint8_t(CodeUpdateState::END));
                    // TODO Set new Dbus property provided by code update app
                    // sendCodeUpdateEvent(effecterId, ACCEPT, END);
                }
                else if (stateField[currState].effecter_state ==
                         uint8_t(CodeUpdateState::REJECT))
                {
                    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
                    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                                         uint8_t(CodeUpdateState::REJECT),
                                         uint8_t(CodeUpdateState::END));
                    // TODO Set new Dbus property provided by code update app
                    // sendCodeUpdateEvent(effecterId, REJECT, END);
                }
            }
            else if (entityType == PLDM_ENTITY_SYSTEM_CHASSIS &&
                     stateSetId == PLDM_OEM_IBM_SYSTEM_POWER_STATE)
            {
                if (stateField[currState].effecter_state == POWER_CYCLE_HARD)
                {
                    systemRebootEvent =
                        std::make_unique<sdeventplus::source::Defer>(
                            event,
                            std::bind(std::mem_fn(&oem_ibm_platform::Handler::
                                                      _processSystemReboot),
                                      this, std::placeholders::_1));
                }
            }
            else
            {
                rc = PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE;
            }
        }
        if (rc != PLDM_SUCCESS)
        {
            break;
        }
    }
    return rc;
}

void buildAllCodeUpdateEffecterPDR(oem_ibm_platform::Handler* platformHandler,
                                   uint16_t entityType, uint16_t entityInstance,
                                   uint16_t stateSetID, pdr_utils::Repo& repo)
{
    size_t pdrSize = 0;
    pdrSize = sizeof(pldm_state_effecter_pdr) +
              sizeof(state_effecter_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(entry.data());
    if (!pdr)
    {
        std::cerr << "Failed to get record by PDR type, ERROR:"
                  << PLDM_PLATFORM_INVALID_EFFECTER_ID << std::endl;
        return;
    }
    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_effecter_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = TERMINUS_HANDLE;
    pdr->effecter_id = platformHandler->getNextEffecterId();
    pdr->entity_type = entityType;
    pdr->entity_instance = entityInstance;
    pdr->container_id = 1;
    pdr->effecter_semantic_id = 0;
    pdr->effecter_init = PLDM_NO_INIT;
    pdr->has_description_pdr = false;
    pdr->composite_effecter_count = 1;

    auto* possibleStatesPtr = pdr->possible_states;
    auto possibleStates =
        reinterpret_cast<state_effecter_possible_states*>(possibleStatesPtr);
    possibleStates->state_set_id = stateSetID;
    possibleStates->possible_states_size = 2;
    auto state =
        reinterpret_cast<state_effecter_possible_states*>(possibleStates);
    if (stateSetID == PLDM_OEM_IBM_BOOT_STATE)
        state->states[0].byte = 6;
    else if (stateSetID == PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
        state->states[0].byte = 126;
    else if (stateSetID == PLDM_OEM_IBM_SYSTEM_POWER_STATE)
        state->states[0].byte = 2;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void buildAllCodeUpdateSensorPDR(oem_ibm_platform::Handler* platformHandler,
                                 uint16_t entityType, uint16_t entityInstance,
                                 uint16_t stateSetID, pdr_utils::Repo& repo)
{
    size_t pdrSize = 0;
    pdrSize =
        sizeof(pldm_state_sensor_pdr) + sizeof(state_sensor_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_sensor_pdr* pdr =
        reinterpret_cast<pldm_state_sensor_pdr*>(entry.data());
    if (!pdr)
    {
        std::cerr << "Failed to get record by PDR type, ERROR:"
                  << PLDM_PLATFORM_INVALID_SENSOR_ID << std::endl;
        return;
    }
    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_SENSOR_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_sensor_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = TERMINUS_HANDLE;
    pdr->sensor_id = platformHandler->getNextSensorId();
    pdr->entity_type = entityType;
    pdr->entity_instance = entityInstance;
    pdr->container_id = 1;
    pdr->sensor_init = PLDM_NO_INIT;
    pdr->sensor_auxiliary_names_pdr = false;
    pdr->composite_sensor_count = 1;

    auto* possibleStatesPtr = pdr->possible_states;
    auto possibleStates =
        reinterpret_cast<state_sensor_possible_states*>(possibleStatesPtr);
    possibleStates->state_set_id = stateSetID;
    possibleStates->possible_states_size = 2;
    auto state =
        reinterpret_cast<state_sensor_possible_states*>(possibleStates);
    if ((stateSetID == PLDM_OEM_IBM_BOOT_STATE) ||
        (stateSetID == PLDM_OEM_IBM_VERIFICATION_STATE))
        state->states[0].byte = 6;
    else if (stateSetID == PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void buildAllNumericEffecterPDR(oem_ibm_platform::Handler* platformHandler,
                                uint16_t entityType, uint16_t entityInstance,
                                uint16_t effecterSemanticId,
                                pdr_utils::Repo& repo,
                                HostEffecterInstanceMap& instanceMap)
{
    size_t pdrSize = 0;
    pdrSize = sizeof(pldm_numeric_effecter_value_pdr);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_numeric_effecter_value_pdr* pdr =
        reinterpret_cast<pldm_numeric_effecter_value_pdr*>(entry.data());
    if (!pdr)
    {
        std::cerr << "Failed to get record by PDR type, ERROR:"
                  << PLDM_PLATFORM_INVALID_EFFECTER_ID << std::endl;
        return;
    }

    std::vector<std::string> procObjectPaths;
    procObjectPaths = getProcObjectPaths();
    for (const auto& entity_path : procObjectPaths)
    {
        pdr->hdr.record_handle = 0;
        pdr->hdr.version = 1;
        pdr->hdr.type = PLDM_NUMERIC_EFFECTER_PDR;
        pdr->hdr.record_change_num = 0;
        pdr->hdr.length =
            sizeof(pldm_numeric_effecter_value_pdr) - sizeof(pldm_pdr_hdr);
        pdr->terminus_handle = TERMINUS_HANDLE;
        pdr->effecter_id = platformHandler->getNextEffecterId();

        uint16_t effecterId = pdr->effecter_id;

        pdr->entity_type = entityType;

        if (entity_path.rfind('/') != std::string::npos)
        {
            char pId = entity_path.back();
            auto procId = pId - 48;
            char id = entity_path.at(61);
            auto dcmId = id - 48;

            InstanceInfo info{};
            info.procId = procId;
            info.dcmId = dcmId;
            instanceMap.emplace(effecterId, info);

            entityInstance = procId;
        }

        pdr->entity_instance = entityInstance;
        pdr->container_id = 3;
        pdr->effecter_semantic_id = effecterSemanticId;
        pdr->effecter_init = PLDM_NO_INIT;
        pdr->effecter_auxiliary_names = false;
        pdr->base_unit = 0;
        pdr->unit_modifier = 0;
        pdr->rate_unit = 0;
        pdr->base_oem_unit_handle = 0;
        pdr->aux_unit = 0;
        pdr->aux_unit_modifier = 0;
        pdr->aux_oem_unit_handle = 0;
        pdr->aux_rate_unit = 0;
        pdr->is_linear = true;
        pdr->effecter_data_size = PLDM_EFFECTER_DATA_SIZE_UINT32;
        pdr->resolution = 1.00;
        pdr->offset = 0.00;
        pdr->accuracy = 0;
        pdr->plus_tolerance = 0;
        pdr->minus_tolerance = 0;
        pdr->state_transition_interval = 0.00;
        pdr->transition_interval = 0.00;
        pdr->max_set_table.value_u32 = 0xFFFFFFFF;
        pdr->min_set_table.value_u32 = 0x0;
        pdr->range_field_format = 0;
        pdr->range_field_support.byte = 0;
        pdr->nominal_value.value_u32 = 0;
        pdr->normal_max.value_u32 = 0;
        pdr->normal_min.value_u32 = 0;
        pdr->rated_max.value_u32 = 0;
        pdr->rated_min.value_u32 = 0;

        pldm::responder::pdr_utils::PdrEntry pdrEntry{};
        pdrEntry.data = entry.data();
        pdrEntry.size = pdrSize;
        repo.addRecord(pdrEntry);
    }
}

void pldm::responder::oem_ibm_platform::Handler::buildOEMPDR(
    pdr_utils::Repo& repo)
{
    buildAllCodeUpdateEffecterPDR(this, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
                                  ENTITY_INSTANCE_0, PLDM_OEM_IBM_BOOT_STATE,
                                  repo);
    buildAllCodeUpdateEffecterPDR(this, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
                                  ENTITY_INSTANCE_1, PLDM_OEM_IBM_BOOT_STATE,
                                  repo);
    buildAllCodeUpdateEffecterPDR(this, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
                                  ENTITY_INSTANCE_0,
                                  PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, repo);
    buildAllCodeUpdateEffecterPDR(this, PLDM_ENTITY_SYSTEM_CHASSIS,
                                  ENTITY_INSTANCE_0,
                                  PLDM_OEM_IBM_SYSTEM_POWER_STATE, repo);

    buildAllCodeUpdateSensorPDR(this, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
                                ENTITY_INSTANCE_0, PLDM_OEM_IBM_BOOT_STATE,
                                repo);
    buildAllCodeUpdateSensorPDR(this, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
                                ENTITY_INSTANCE_1, PLDM_OEM_IBM_BOOT_STATE,
                                repo);
    buildAllCodeUpdateSensorPDR(this, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
                                ENTITY_INSTANCE_0,
                                PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, repo);
    buildAllCodeUpdateSensorPDR(this, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
                                ENTITY_INSTANCE_0,
                                PLDM_OEM_IBM_VERIFICATION_STATE, repo);

    buildAllNumericEffecterPDR(this, PLDM_ENTITY_PROC, ENTITY_INSTANCE_0,
                               PLDM_OEM_IBM_SBE_SEMANTIC_ID, repo, instanceMap);

    auto sensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
        ENTITY_INSTANCE_0, 1, PLDM_OEM_IBM_VERIFICATION_STATE);
    codeUpdate->setMarkerLidSensor(sensorId);
    sensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
        ENTITY_INSTANCE_0, 1, PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE);
    codeUpdate->setFirmwareUpdateSensor(sensorId);
}

void pldm::responder::oem_ibm_platform::Handler::setPlatformHandler(
    pldm::responder::platform::Handler* handler)
{
    platformHandler = handler;
}

int pldm::responder::oem_ibm_platform::Handler::sendEventToHost(
    std::vector<uint8_t>& requestMsg, uint8_t instanceId)
{
    if (requestMsg.size())
    {
        std::ostringstream tempStream;
        for (int byte : requestMsg)
        {
            tempStream << std::setfill('0') << std::setw(2) << std::hex << byte
                       << " ";
        }
        std::cout << tempStream.str() << std::endl;
    }
    auto oemPlatformEventMessageResponseHandler =
        [](mctp_eid_t /*eid*/, const pldm_msg* response, size_t respMsgLen) {
            uint8_t completionCode{};
            uint8_t status{};
            auto rc = decode_platform_event_message_resp(
                response, respMsgLen, &completionCode, &status);
            if (rc || completionCode)
            {
                std::cerr << "Failed to decode_platform_event_message_resp: "
                          << " for code update event rc=" << rc
                          << ", cc=" << static_cast<unsigned>(completionCode)
                          << std::endl;
            }
        };
    auto rc = handler->registerRequest(
        mctp_eid, instanceId, PLDM_PLATFORM, PLDM_PLATFORM_EVENT_MESSAGE,
        std::move(requestMsg),
        std::move(oemPlatformEventMessageResponseHandler));
    if (rc)
    {
        std::cerr << "Failed to send BIOS attribute change event message \n";
    }

    return rc;
}

int encodeEventMsg(uint8_t eventType, const std::vector<uint8_t>& eventDataVec,
                   std::vector<uint8_t>& requestMsg, uint8_t instanceId)
{
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_platform_event_message_req(
        instanceId, 1 /*formatVersion*/, 0 /*tId*/, eventType,
        eventDataVec.data(), eventDataVec.size(), request,
        eventDataVec.size() + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES);

    return rc;
}
void pldm::responder::oem_ibm_platform::Handler::setHostEffecterState(
    bool status)
{
    pldm::pdr::EntityType entityType = PLDM_ENTITY_PROC;
    pldm::pdr::StateSetId stateSetId = PLDM_OEM_IBM_SBE_SEMANTIC_ID;

    uint8_t tid = TERMINUS_ID;

    auto pdrs = findStateEffecterPDR(tid, entityType, stateSetId, pdrRepo);
    for (auto& pdr : pdrs)
    {
        auto stateEffecterPDR =
            reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());
        uint16_t effecterId = stateEffecterPDR->effecter_id;
        uint8_t compEffecterCount = stateEffecterPDR->composite_effecter_count;

        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + sizeof(effecterId) +
                sizeof(compEffecterCount) +
                sizeof(set_effecter_state_field) * compEffecterCount,
            0);

        auto instanceId = requester.getInstanceId(mctp_eid);

        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        std::vector<set_effecter_state_field> stateField;
        if (status == true)
        {
            stateField.push_back(
                set_effecter_state_field{PLDM_REQUEST_SET, SBE_DUMP_COMPLETED});
        }
        else
        {
            stateField.push_back(
                set_effecter_state_field{PLDM_REQUEST_SET, SBE_RETRY_REQUIRED});
        }
        auto rc = encode_set_state_effecter_states_req(
            instanceId, effecterId, compEffecterCount, stateField.data(),
            request);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr
                << " Set state effecter state command failure. PLDM error code ="
                << rc << std::endl;
            requester.markFree(mctp_eid, instanceId);
            return;
        }
        auto setStateEffecterStatesRespHandler =
            [=, this](mctp_eid_t /*eid*/, const pldm_msg* response,
                      size_t respMsgLen) {
                if (response == nullptr || !respMsgLen)
                {
                    std::cerr << "Failed to receive response for "
                              << "setstateEffecterSates command\n";
                    return;
                }
                uint8_t completionCode{};
                auto rc = decode_set_state_effecter_states_resp(
                    response, respMsgLen, &completionCode);
                if (rc)
                {
                    std::cerr
                        << "Failed to decode setStateEffecterStates response,"
                        << " rc " << rc << "\n";
                    pldm::utils::reportError(
                        "xyz.openbmc_project.bmc.pldm.SetHostEffecterFailed");
                }
                if (completionCode)
                {
                    std::cerr
                        << "Failed to set a Host effecter "
                        << ", cc=" << static_cast<unsigned>(completionCode)
                        << "\n";
                    pldm::utils::reportError(
                        "xyz.openbmc_project.bmc.pldm.SetHostEffecterFailed");
                }
            };
        rc = handler->registerRequest(
            mctp_eid, instanceId, PLDM_PLATFORM, PLDM_SET_STATE_EFFECTER_STATES,
            std::move(requestMsg),
            std::move(setStateEffecterStatesRespHandler));
        if (rc)
        {
            std::cerr << "Failed to send request to set an effecter on Host \n";
        }
    }
}
void pldm::responder::oem_ibm_platform::Handler::monitorDump(
    const std::string& obj_path)
{

    std::string matchInterface = "xyz.openbmc_project.Common.Progress";
    sbeDumpMatch = std::make_unique<sdbusplus::bus::match::match>(
        pldm::utils::DBusHandler::getBus(),
        sdbusplus::bus::match::rules::propertiesChanged(obj_path.c_str(),
                                                        matchInterface.c_str()),
        [&](sdbusplus::message::message& msg) {
            DbusChangedProps props{};
            std::string intf;
            msg.read(intf, props);
            const auto itr = props.find("Status");
            if (itr != props.end())
            {
                PropertyValue value = itr->second;
                auto propVal = std::get<std::string>(value);
                if (propVal ==
                    "xyz.openbmc_project.Common.Progress.OperationStatus.Completed")
                {
                    setHostEffecterState(true);
                }
                else if (
                    propVal ==
                        "xyz.openbmc_project.Common.Progress.OperationStatus.Failed" ||
                    propVal ==
                        "xyz.openbmc_project.Common.Progress.OperationStatus.Aborted")
                {
                    setHostEffecterState(false);
                }
            }
            sbeDumpMatch = nullptr;
        });
}

int pldm::responder::oem_ibm_platform::Handler::setNumericEffecter(
    uint16_t entityInstance, const PropertyValue& propertyValue)
{
    static constexpr auto objectPath = "/org/openpower/dump";
    static constexpr auto interface = "xyz.openbmc_project.Dump.Create";

    uint32_t value = std::get<uint32_t>(propertyValue);
    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        auto service =
            pldm::utils::DBusHandler().getService(objectPath, interface);
        auto method = bus.new_method_call(service.c_str(), objectPath,
                                          interface, "CreateDump");

        std::map<std::string, std::variant<std::string, uint64_t>> createParams;
        createParams["com.ibm.Dump.Create.CreateParameters.DumpType"] =
            "com.ibm.Dump.Create.DumpType.SBE";
        createParams["com.ibm.Dump.Create.CreateParameters.ErrorLogId"] =
            (uint64_t)value;
        createParams["com.ibm.Dump.Create.CreateParameters.FailingUnitId"] =
            (uint64_t)entityInstance;
        method.append(createParams);

        auto response = bus.call(method);

        sdbusplus::message::object_path reply;
        response.read(reply);

        monitorDump(reply);
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "Failed to make a DBus call as the dump policy is disabled,ERROR= "
            << e.what() << "\n";
        // case when the dump policy is disabled but we set the host effecter as
        // true and the host moves on
        setHostEffecterState(true);
    }
    return PLDM_SUCCESS;
}

std::vector<std::string> getProcObjectPaths()
{
    static constexpr auto searchpath = "/xyz/openbmc_project/inventory/system";
    int depth = 0;
    std::vector<std::string> procInterface = {
        "xyz.openbmc_project.Inventory.Item.Cpu"};
    pldm::utils::MapperGetSubTreeResponse response =
        pldm::utils::DBusHandler().getSubtree(searchpath, depth, procInterface);
    std::vector<std::string> procPaths;
    for (const auto& [objPath, serviceMap] : response)
    {
        procPaths.emplace_back(objPath);
    }
    return procPaths;
}

void pldm::responder::oem_ibm_platform::Handler::sendStateSensorEvent(
    uint16_t sensorId, enum sensor_event_class_states sensorEventClass,
    uint8_t sensorOffset, uint8_t eventState, uint8_t prevEventState)
{
    std::vector<uint8_t> sensorEventDataVec{};
    size_t sensorEventSize = PLDM_SENSOR_EVENT_DATA_MIN_LENGTH + 1;
    sensorEventDataVec.resize(sensorEventSize);
    auto eventData = reinterpret_cast<struct pldm_sensor_event_data*>(
        sensorEventDataVec.data());
    eventData->sensor_id = sensorId;
    eventData->sensor_event_class_type = sensorEventClass;
    auto eventClassStart = eventData->event_class;
    auto eventClass =
        reinterpret_cast<struct pldm_sensor_event_state_sensor_state*>(
            eventClassStart);
    eventClass->sensor_offset = sensorOffset;
    eventClass->event_state = eventState;
    eventClass->previous_event_state = prevEventState;
    auto instanceId = requester.getInstanceId(mctp_eid);
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
                                    sensorEventDataVec.size());
    auto rc = encodeEventMsg(PLDM_SENSOR_EVENT, sensorEventDataVec, requestMsg,
                             instanceId);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to encode state sensor event, rc = " << rc
                  << std::endl;
        requester.markFree(mctp_eid, instanceId);
        return;
    }
    rc = sendEventToHost(requestMsg, instanceId);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Failed to send event to host: "
                  << "rc=" << rc << std::endl;
    }
    return;
}

void pldm::responder::oem_ibm_platform::Handler::_processEndUpdate(
    sdeventplus::source::EventBase& /*source */)
{
    assembleImageEvent.reset();
    int retc = codeUpdate->assembleCodeUpdateImage();
    if (retc != PLDM_SUCCESS)
    {
        codeUpdate->setCodeUpdateProgress(false);
        auto sensorId = codeUpdate->getFirmwareUpdateSensor();
        sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                             uint8_t(CodeUpdateState::FAIL),
                             uint8_t(CodeUpdateState::START));
    }
}

void pldm::responder::oem_ibm_platform::Handler::_processStartUpdate(
    sdeventplus::source::EventBase& /*source */)
{
    codeUpdate->deleteImage();
    CodeUpdateState state = CodeUpdateState::START;
    auto rc = codeUpdate->setRequestedApplyTime();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "setRequestedApplyTime failed \n";
        state = CodeUpdateState::FAIL;
    }
    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0, uint8_t(state),
                         uint8_t(CodeUpdateState::END));
}

void pldm::responder::oem_ibm_platform::Handler::_processSystemReboot(
    sdeventplus::source::EventBase& /*source */)
{
    pldm::utils::PropertyValue value =
        "xyz.openbmc_project.State.Chassis.Transition.Off";
    pldm::utils::DBusMapping dbusMapping{"/xyz/openbmc_project/state/chassis0",
                                         "xyz.openbmc_project.State.Chassis",
                                         "RequestedPowerTransition", "string"};
    try
    {
        dBusIntf->setDbusProperty(dbusMapping, value);
    }
    catch (const std::exception& e)
    {

        std::cerr << "Chassis State transition to Off failed,"
                  << "unable to set property RequestedPowerTransition"
                  << "ERROR=" << e.what() << "\n";
    }

    using namespace sdbusplus::bus::match::rules;
    chassisOffMatch = std::make_unique<sdbusplus::bus::match::match>(
        pldm::utils::DBusHandler::getBus(),
        propertiesChanged("/xyz/openbmc_project/state/chassis0",
                          "xyz.openbmc_project.State.Chassis"),
        [this](sdbusplus::message::message& msg) {
            DbusChangedProps props{};
            std::string intf;
            msg.read(intf, props);
            const auto itr = props.find("CurrentPowerState");
            if (itr != props.end())
            {
                PropertyValue value = itr->second;
                auto propVal = std::get<std::string>(value);
                if (propVal ==
                    "xyz.openbmc_project.State.Chassis.PowerState.Off")
                {
                    pldm::utils::DBusMapping dbusMapping{
                        "/xyz/openbmc_project/control/host0/"
                        "power_restore_policy/one_time",
                        "xyz.openbmc_project.Control.Power.RestorePolicy",
                        "PowerRestorePolicy", "string"};
                    value = "xyz.openbmc_project.Control.Power.RestorePolicy."
                            "Policy.AlwaysOn";
                    try
                    {
                        dBusIntf->setDbusProperty(dbusMapping, value);
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Setting one-time restore policy failed,"
                                  << "unable to set property PowerRestorePolicy"
                                  << "ERROR=" << e.what() << "\n";
                    }
                    dbusMapping = pldm::utils::DBusMapping{
                        "/xyz/openbmc_project/state/bmc0",
                        "xyz.openbmc_project.State.BMC",
                        "RequestedBMCTransition", "string"};
                    value = "xyz.openbmc_project.State.BMC.Transition.Reboot";
                    try
                    {
                        dBusIntf->setDbusProperty(dbusMapping, value);
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "BMC state transition to reboot failed,"
                                  << "unable to set property "
                                     "RequestedBMCTransition"
                                  << "ERROR=" << e.what() << "\n";
                    }
                }
            }
        });
}

void pldm::responder::oem_ibm_platform::Handler::checkAndDisableWatchDog()
{
    if (!hostOff && setEventReceiverCnt == SET_EVENT_RECEIVER_SENT)
    {
        disableWatchDogTimer();
    }

    return;
}

bool pldm::responder::oem_ibm_platform::Handler::watchDogRunning()
{
    static constexpr auto watchDogObjectPath =
        "/xyz/openbmc_project/watchdog/host0";
    static constexpr auto watchDogEnablePropName = "Enabled";
    static constexpr auto watchDogInterface =
        "xyz.openbmc_project.State.Watchdog";
    bool isWatchDogRunning = false;
    try
    {
        isWatchDogRunning = pldm::utils::DBusHandler().getDbusProperty<bool>(
            watchDogObjectPath, watchDogEnablePropName, watchDogInterface);
    }
    catch (const std::exception& e)
    {
        return false;
    }
    return isWatchDogRunning;
}

void pldm::responder::oem_ibm_platform::Handler::resetWatchDogTimer()
{
    static constexpr auto watchDogService = "xyz.openbmc_project.Watchdog";
    static constexpr auto watchDogObjectPath =
        "/xyz/openbmc_project/watchdog/host0";
    static constexpr auto watchDogInterface =
        "xyz.openbmc_project.State.Watchdog";
    static constexpr auto watchDogResetPropName = "ResetTimeRemaining";

    bool wdStatus = watchDogRunning();
    if (wdStatus == false)
    {
        return;
    }
    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        auto resetMethod =
            bus.new_method_call(watchDogService, watchDogObjectPath,
                                watchDogInterface, watchDogResetPropName);
        resetMethod.append(true);
        bus.call_noreply(resetMethod);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed To reset watchdog timer"
                  << "ERROR=" << e.what() << std::endl;
        return;
    }
}

void pldm::responder::oem_ibm_platform::Handler::disableWatchDogTimer()
{
    setEventReceiverCnt = 0;
    pldm::utils::DBusMapping dbusMapping{"/xyz/openbmc_project/watchdog/host0",
                                         "xyz.openbmc_project.State.Watchdog",
                                         "Enabled", "bool"};
    bool wdStatus = watchDogRunning();

    if (!wdStatus)
    {
        return;
    }
    try
    {
        pldm::utils::DBusHandler().setDbusProperty(dbusMapping, false);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed To disable watchdog timer"
                  << "ERROR=" << e.what() << "\n";
    }
}
int pldm::responder::oem_ibm_platform::Handler::checkBMCState()
{
    try
    {
        pldm::utils::PropertyValue propertyValue =
            pldm::utils::DBusHandler().getDbusPropertyVariant(
                "/xyz/openbmc_project/state/bmc0", "CurrentBMCState",
                "xyz.openbmc_project.State.BMC");

        if (std::get<std::string>(propertyValue) ==
            "xyz.openbmc_project.State.BMC.BMCState.NotReady")
        {
            std::cerr << "GetPDR : PLDM stack is not ready for PDR exchange"
                      << std::endl;
            return PLDM_ERROR_NOT_READY;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error getting the current BMC state" << std::endl;
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

void pldm::responder::oem_ibm_platform::Handler::updateContainerID()
{
    for (auto& [key, value] : instanceMap)
    {
        uint16_t newContainerID = pldm_find_container_id(
            pdrRepo, PLDM_ENTITY_PROC_MODULE, value.dcmId);
        pldm_change_container_id_of_effecter(pdrRepo, key, newContainerID);
    }
}

} // namespace oem_ibm_platform
} // namespace responder
} // namespace pldm
