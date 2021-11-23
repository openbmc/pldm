#include "oem_ibm_handler.hpp"

#include "libpldm/entity.h"
#include "libpldm/requester/pldm.h"

#include "collect_slot_vpd.hpp"
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
    getOemStateSensorReadingsHandler(
        EntityType entityType, EntityInstance entityInstance,
        ContainerID containerId, StateSetId stateSetId,
        CompositeCount compSensorCnt, uint16_t /*sensorId*/,
        std::vector<get_sensor_state_field>& stateField)
{
    auto& entityAssociationMap = getAssociateEntityMap();
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
        else if (entityType == PLDM_ENTITY_SLOT &&
                 stateSetId == PLDM_OEM_IBM_SLOT_ENABLE_SENSOR_STATE)
        {
            for (const auto& [key, value] : entityAssociationMap)
            {
                if (value.entity_type == entityType &&
                    value.entity_instance_num == entityInstance &&
                    value.entity_container_id == containerId)
                {
                    sensorOpState = slotHandler->fetchSlotSensorState(key);
                    break;
                }
            }
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
        std::vector<set_effecter_state_field>& stateField, uint16_t effecterId)
{
    int rc = PLDM_SUCCESS;
    auto& entityAssociationMap = getAssociateEntityMap();

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
            else if (stateSetId == PLDM_OEM_IBM_SLOT_ENABLE_EFFECTER_STATE)
            {
                std::cerr << "Effecter id : " << effecterId << std::endl;
                slotHandler->enableSlot(effecterId, entityAssociationMap,
                                        stateField[currState].effecter_state);
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

void buildAllSlotEnabeEffecterPDR(oem_ibm_platform::Handler* platformHandler,
                                  pdr_utils::Repo& repo,
                                  const std::vector<std::string>& slotobjpaths)
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

    auto& associatedEntityMap = platformHandler->getAssociateEntityMap();
    for (const auto& entity_path : slotobjpaths)
    {
        pdr->hdr.record_handle = 0;
        pdr->hdr.version = 1;
        pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
        pdr->hdr.record_change_num = 0;
        pdr->hdr.length =
            sizeof(pldm_state_effecter_pdr) - sizeof(pldm_pdr_hdr);
        pdr->terminus_handle = TERMINUS_HANDLE;
        pdr->effecter_id = platformHandler->getNextEffecterId();

        if (entity_path != "" &&
            associatedEntityMap.find(entity_path) != associatedEntityMap.end())
        {
            pdr->entity_type = associatedEntityMap.at(entity_path).entity_type;
            pdr->entity_instance =
                associatedEntityMap.at(entity_path).entity_instance_num;
            pdr->container_id =
                associatedEntityMap.at(entity_path).entity_container_id;
            platformHandler->effecterIdToDbusMap[pdr->effecter_id] =
                entity_path;
        }
        else
        {
            // the slots are not present, dont create the PDR
            continue;
        }
        pdr->effecter_semantic_id = 0;
        pdr->effecter_init = PLDM_NO_INIT;
        pdr->has_description_pdr = false;
        pdr->composite_effecter_count = 1;

        auto* possibleStatesPtr = pdr->possible_states;
        auto possibleStates = reinterpret_cast<state_effecter_possible_states*>(
            possibleStatesPtr);
        possibleStates->state_set_id = PLDM_OEM_IBM_SLOT_ENABLE_EFFECTER_STATE;
        possibleStates->possible_states_size = 2;
        auto state =
            reinterpret_cast<state_effecter_possible_states*>(possibleStates);
        state->states[0].byte = 14;
        pldm::responder::pdr_utils::PdrEntry pdrEntry{};
        pdrEntry.data = entry.data();
        pdrEntry.size = pdrSize;
        repo.addRecord(pdrEntry);
    }
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
        (stateSetID == PLDM_OEM_IBM_VERIFICATION_STATE) ||
        (stateSetID == PLDM_OEM_IBM_BOOT_SIDE_RENAME))
        state->states[0].byte = 6;
    else if (stateSetID == PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void buildAllSlotEnableSensorPDR(oem_ibm_platform::Handler* platformHandler,
                                 pdr_utils::Repo& repo,
                                 const std::vector<std::string>& slotobjpaths)
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
    auto& associatedEntityMap = platformHandler->getAssociateEntityMap();
    for (const auto& entity_path : slotobjpaths)
    {
        pdr->hdr.record_handle = 0;
        pdr->hdr.version = 1;
        pdr->hdr.type = PLDM_STATE_SENSOR_PDR;
        pdr->hdr.record_change_num = 0;
        pdr->hdr.length = sizeof(pldm_state_sensor_pdr) - sizeof(pldm_pdr_hdr);
        pdr->terminus_handle = TERMINUS_HANDLE;
        pdr->sensor_id = platformHandler->getNextSensorId();
        if (entity_path != "" &&
            associatedEntityMap.find(entity_path) != associatedEntityMap.end())
        {
            pdr->entity_type = associatedEntityMap.at(entity_path).entity_type;
            pdr->entity_instance =
                associatedEntityMap.at(entity_path).entity_instance_num;
            pdr->container_id =
                associatedEntityMap.at(entity_path).entity_container_id;
        }
        else
        {
            // the slots are not present, dont create the PDR
            continue;
        }

        pdr->sensor_init = PLDM_NO_INIT;
        pdr->sensor_auxiliary_names_pdr = false;
        pdr->composite_sensor_count = 1;

        auto* possibleStatesPtr = pdr->possible_states;
        auto possibleStates =
            reinterpret_cast<state_sensor_possible_states*>(possibleStatesPtr);
        possibleStates->state_set_id = PLDM_OEM_IBM_SLOT_ENABLE_SENSOR_STATE;
        possibleStates->possible_states_size = 1;
        auto state =
            reinterpret_cast<state_sensor_possible_states*>(possibleStates);
        state->states[0].byte = 15;
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
    std::vector<std::string> slotobjpaths = {
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot0",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot1",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot10",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot11",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot2",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot3",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot4",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot5",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot6",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot7",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot8",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/pcieslot9"};
    buildAllSlotEnabeEffecterPDR(this, repo, slotobjpaths);
    buildAllSlotEnableSensorPDR(this, repo, slotobjpaths);

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
    buildAllCodeUpdateSensorPDR(this, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
                                ENTITY_INSTANCE_0,
                                PLDM_OEM_IBM_BOOT_SIDE_RENAME, repo);
    auto sensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
        ENTITY_INSTANCE_0, 1, PLDM_OEM_IBM_VERIFICATION_STATE);
    codeUpdate->setMarkerLidSensor(sensorId);
    sensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
        ENTITY_INSTANCE_0, 1, PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE);
    codeUpdate->setFirmwareUpdateSensor(sensorId);
    sensorId =
        findStateSensorId(repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
                          ENTITY_INSTANCE_0, 0, PLDM_OEM_IBM_BOOT_SIDE_RENAME);
    codeUpdate->setBootSideRenameStateSensor(sensorId);
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

void pldm::responder::oem_ibm_platform::Handler::upadteOemDbusPaths(
    std::string& dbusPath)
{
    std::string toFind("system1/chassis1/motherboard1");
    if (dbusPath.find(toFind) != std::string::npos)
    {
        size_t pos = dbusPath.find(toFind);
        dbusPath.replace(pos, toFind.length(), "system/chassis/motherboard");
    }
    toFind = "system1";
    if (dbusPath.find(toFind) != std::string::npos)
    {
        size_t pos = dbusPath.find(toFind);
        dbusPath.replace(pos, toFind.length(), "system");
    }
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

void pldm::responder::oem_ibm_platform::Handler::modifyPDROemActions(
    uint32_t /*recordHandle*/, pldm_pdr* repo)
{
    // need to add a change here to compare the recordHandle before fetching the
    // panel effecter and calling panel
    auto pdrs = pldm::utils::findStateEffecterPDR(
        0xD0,
        pldm::responder::oem_ibm_platform::PLDM_OEM_IBM_FRONT_PANEL_TRIGGER,
        PLDM_OEM_IBM_PANEL_TRIGGER_STATE, repo);
    if (!std::empty(pdrs))
    {
        auto bitMap = responder::pdr_utils::fetchBitMap(pdrs);
        pldm::utils::dbusMethodCall("com.ibm.PanelApp", "/com/ibm/panel_app",
                                    "toggleFunctionState", "com.ibm.panel",
                                    bitMap);
    }
}

void pldm::responder::oem_ibm_platform::Handler::handleBootTypesAtPowerOn()
{
    auto bootInitiator = getBiosAttrValue("pvm_boot_initiator_current");
    if (bootInitiator != "HMC" && !bootInitiator.empty())
    {
        auto restartCause =
            pldm::utils::DBusHandler().getDbusProperty<std::string>(
                "/xyz/openbmc_project/state/host0", "RestartCause",
                "xyz.openbmc_project.State.Host");

        if (restartCause ==
            "xyz.openbmc_project.State.Host.RestartCause.ScheduledPowerOn")
        {
            setBiosAttr("pvm_boot_initiator", "Host");
        }
        else if (
            (restartCause ==
             "xyz.openbmc_project.State.Host.RestartCause.PowerPolicyAlwaysOn") ||
            (restartCause ==
             "xyz.openbmc_project.State.Host.RestartCause.PowerPolicyPreviousState") ||
            (restartCause ==
             "xyz.openbmc_project.State.Host.RestartCause.HostCrash"))
        {
            setBiosAttr("pvm_boot_initiator", "Auto");
        }
    }
}

void pldm::responder::oem_ibm_platform::Handler::handleBootTypesAtChassisOff()
{
    auto bootInitiator = getBiosAttrValue("pvm_boot_initiator");
    auto bootType = getBiosAttrValue("pvm_boot_type");
    if (bootInitiator.empty() || bootType.empty())
    {
        std::cerr
            << "ERROR in fetching the pvm_boot_initiator and pvm_boot_type BIOS attribute values\n";
    }
    else if ((bootInitiator == "Auto") && (bootType == "IPL"))
    {
        std::cerr << "Setting the APR to AlwaysOn as pvm_boot_initiator="
                  << bootInitiator << " and pvm_boot_type=" << bootType
                  << std::endl;
        pldm::utils::DBusMapping dbusMapping{
            "/xyz/openbmc_project/control/host0/"
            "power_restore_policy/one_time",
            "xyz.openbmc_project.Control.Power.RestorePolicy",
            "PowerRestorePolicy", "string"};
        PropertyValue value = "xyz.openbmc_project.Control.Power.RestorePolicy."
                              "Policy.AlwaysOn";
        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Setting one-time restore policy failed,"
                      << "unable to set property PowerRestorePolicy"
                      << "ERROR=" << e.what() << "\n";
        }
    }
    else
    {
        setBiosAttr("pvm_boot_initiator", "User");
        setBiosAttr("pvm_boot_type", "IPL");
    }
}

} // namespace oem_ibm_platform
} // namespace responder
} // namespace pldm
