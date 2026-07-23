#include "oem_ibm_handler.hpp"

#include "collect_slot_vpd.hpp"
#include "common/types.hpp"
#include "file_io_type_lid.hpp"
#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/pdr_utils.hpp"

#include <libpldm/entity.h>
#include <libpldm/oem/ibm/entity.h>
#include <libpldm/pldm.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/State/BMC/client.hpp>
#include <xyz/openbmc_project/State/Host/common.hpp>

PHOSPHOR_LOG2_USING;

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
        pldm::pdr::EntityType entityType, EntityInstance entityInstance,
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
        uint8_t presentState = PLDM_SENSOR_UNKNOWN;
        if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
            stateSetId == PLDM_OEM_IBM_BOOT_STATE)
        {
            sensorOpState = fetchBootSide(entityInstance, codeUpdate);
        }
        else if (entityType == PLDM_ENTITY_SLOT &&
                 stateSetId == PLDM_OEM_IBM_PCIE_SLOT_SENSOR_STATE)
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
        else if (entityType == PLDM_OEM_IBM_ENTITY_REAL_SAI &&
                 stateSetId == PLDM_STATE_SET_OPERATIONAL_FAULT_STATUS)
        {
            sensorOpState = fetchRealSAIStatus();
            presentState = PLDM_SENSOR_NORMAL;
        }
        else
        {
            rc = PLDM_PLATFORM_INVALID_STATE_VALUE;
            break;
        }
        stateField.push_back({PLDM_SENSOR_ENABLED, presentState,
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
            else if (stateSetId == PLDM_OEM_IBM_PCIE_SLOT_EFFECTER_STATE)
            {
                slotHandler->enableSlot(effecterId, entityAssociationMap,
                                        stateField[currState].effecter_state);
            }
            else if (entityType == PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE &&
                     stateSetId == PLDM_OEM_IBM_BOOT_SIDE_RENAME)
            {
                if (stateField[currState].effecter_state ==
                    PLDM_OEM_IBM_BOOT_SIDE_RENAME_STATE_RENAMED)
                {
                    codeUpdate->processRenameEvent();
                }
            }
            else if (entityType == PLDM_OEM_IBM_ENTITY_REAL_SAI &&
                     stateSetId == PLDM_STATE_SET_OPERATIONAL_FAULT_STATUS)
            {
                turnOffRealSAIEffecter();
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

// ---------------------------------------------------------------------------
// PDR descriptor: the fixed parameters needed to build one effecter PDR entry.
// ---------------------------------------------------------------------------
struct EffecterPDRParams
{
    uint16_t entityType;
    uint16_t entityInstance;
    uint16_t stateSetId;
    uint8_t possibleStatesSize;
    uint8_t statesByte;
};

// ---------------------------------------------------------------------------
// PDR descriptor: the fixed parameters needed to build one sensor PDR entry.
// ---------------------------------------------------------------------------
struct SensorPDRParams
{
    uint16_t entityType;
    uint16_t entityInstance;
    uint16_t stateSetId;
    uint8_t possibleStatesSize;
    uint8_t statesByte;
};

// ---------------------------------------------------------------------------
// buildStateEffecterPDR — single shared builder for ALL effecter PDR entries.
//
// Allocates one PDR entry buffer, fills every field from the supplied params,
// and commits it to the repo.  The slot variant passes entity info resolved
// from associatedEntityMap; the regular variant passes it directly.
// ---------------------------------------------------------------------------
static void buildStateEffecterPDR(oem_ibm_platform::Handler* platformHandler,
                                  const EffecterPDRParams& params,
                                  pdr_utils::Repo& repo,
                                  const std::string& dbusPath = "")
{
    const size_t pdrSize = sizeof(pldm_state_effecter_pdr) +
                           sizeof(state_effecter_possible_states);
    std::vector<uint8_t> entry(pdrSize, 0);

    auto* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(entry.data());

    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length =
        sizeof(pldm_state_effecter_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = TERMINUS_HANDLE;
    pdr->effecter_id = platformHandler->getNextEffecterId();
    pdr->entity_type = params.entityType;
    pdr->entity_instance = params.entityInstance;
    pdr->container_id = 1;
    pdr->effecter_semantic_id = 0;
    pdr->effecter_init = PLDM_NO_INIT;
    pdr->has_description_pdr = false;
    pdr->composite_effecter_count = 1;

    auto* possibleStates =
        reinterpret_cast<state_effecter_possible_states*>(
            pdr->possible_states);
    possibleStates->state_set_id = params.stateSetId;
    possibleStates->possible_states_size = params.possibleStatesSize;
    possibleStates->states[0].byte = params.statesByte;

    if (!dbusPath.empty())
    {
        platformHandler->effecterIdToDbusMap[pdr->effecter_id] = dbusPath;
    }

    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

// ---------------------------------------------------------------------------
// buildStateEffecterPDRForSlots — iterates slot object paths, resolves entity
// info from associatedEntityMap, and calls the shared builder for each slot.
// ---------------------------------------------------------------------------
static void buildStateEffecterPDRForSlots(
    oem_ibm_platform::Handler* platformHandler, pdr_utils::Repo& repo,
    const std::vector<std::string>& slotObjPaths)
{
    const auto& associatedEntityMap =
        platformHandler->getAssociateEntityMap();

    for (const auto& entityPath : slotObjPaths)
    {
        if (entityPath.empty() ||
            !associatedEntityMap.contains(entityPath))
        {
            // slot not present — skip
            continue;
        }

        const auto& entity = associatedEntityMap.at(entityPath);
        EffecterPDRParams params{entity.entity_type,
                                 entity.entity_instance_num,
                                 PLDM_OEM_IBM_PCIE_SLOT_EFFECTER_STATE,
                                 2,
                                 14};
        buildStateEffecterPDR(platformHandler, params, repo, entityPath);
    }
}

// ---------------------------------------------------------------------------
// buildStateSensorPDR — single shared builder for ALL sensor PDR entries.
// ---------------------------------------------------------------------------
static void buildStateSensorPDR(oem_ibm_platform::Handler* platformHandler,
                                const SensorPDRParams& params,
                                pdr_utils::Repo& repo)
{
    const size_t pdrSize = sizeof(pldm_state_sensor_pdr) +
                           sizeof(state_sensor_possible_states);
    std::vector<uint8_t> entry(pdrSize, 0);

    auto* pdr =
        reinterpret_cast<pldm_state_sensor_pdr*>(entry.data());

    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_SENSOR_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length =
        sizeof(pldm_state_sensor_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = TERMINUS_HANDLE;
    pdr->sensor_id = platformHandler->getNextSensorId();
    pdr->entity_type = params.entityType;
    pdr->entity_instance = params.entityInstance;
    pdr->container_id = 1;
    pdr->sensor_init = PLDM_NO_INIT;
    pdr->sensor_auxiliary_names_pdr = false;
    pdr->composite_sensor_count = 1;

    auto* possibleStates =
        reinterpret_cast<state_sensor_possible_states*>(
            pdr->possible_states);
    possibleStates->state_set_id = params.stateSetId;
    possibleStates->possible_states_size = params.possibleStatesSize;
    possibleStates->states[0].byte = params.statesByte;

    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

// ---------------------------------------------------------------------------
// buildStateSensorPDRForSlots — iterates slot object paths, resolves entity
// info from associatedEntityMap, and calls the shared builder for each slot.
// ---------------------------------------------------------------------------
static void buildStateSensorPDRForSlots(
    oem_ibm_platform::Handler* platformHandler, pdr_utils::Repo& repo,
    const std::vector<std::string>& slotObjPaths)
{
    const auto& associatedEntityMap =
        platformHandler->getAssociateEntityMap();

    for (const auto& entityPath : slotObjPaths)
    {
        if (entityPath.empty() ||
            !associatedEntityMap.contains(entityPath))
        {
            // slot not present — skip
            continue;
        }

        const auto& entity = associatedEntityMap.at(entityPath);
        SensorPDRParams params{entity.entity_type,
                               entity.entity_instance_num,
                               PLDM_OEM_IBM_PCIE_SLOT_SENSOR_STATE,
                               1,
                               15};
        buildStateSensorPDR(platformHandler, params, repo);
    }
}

void pldm::responder::oem_ibm_platform::Handler::buildOEMPDR(
    pdr_utils::Repo& repo)
{
    // ------------------------------------------------------------------
    // Effecter PDRs
    // ------------------------------------------------------------------

    // Code-update boot-state effecters (instance 0 and 1)
    for (uint16_t instance : {ENTITY_INSTANCE_0, ENTITY_INSTANCE_1})
    {
        buildStateEffecterPDR(
            this,
            {PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE, instance,
             PLDM_OEM_IBM_BOOT_STATE, 2, 6},
            repo);
    }

    // Firmware-update-state effecter
    buildStateEffecterPDR(
        this,
        {PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE, ENTITY_INSTANCE_0,
         PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, 2, 126},
        repo);

    // System power-state effecter
    buildStateEffecterPDR(
        this,
        {PLDM_ENTITY_SYSTEM_CHASSIS, ENTITY_INSTANCE_1,
         PLDM_OEM_IBM_SYSTEM_POWER_STATE, 2, 2},
        repo);

    // PCIe slot effecters (one PDR per discovered slot)
    static constexpr auto inventoryRoot =
        "/xyz/openbmc_project/inventory/system";
    const std::vector<std::string> slotInterface = {
        "xyz.openbmc_project.Inventory.Item.PCIeSlot"};
    auto slotPaths =
        dBusIntf->getSubTreePaths(inventoryRoot, 0, slotInterface);
    buildStateEffecterPDRForSlots(this, repo, slotPaths);

    // Real SAI effecter
    buildStateEffecterPDR(
        this,
        {PLDM_OEM_IBM_ENTITY_REAL_SAI, ENTITY_INSTANCE_1,
         PLDM_STATE_SET_OPERATIONAL_FAULT_STATUS, 1, 2},
        repo);

    // ------------------------------------------------------------------
    // Sensor PDRs
    // ------------------------------------------------------------------

    // PCIe slot sensors (one PDR per discovered slot)
    buildStateSensorPDRForSlots(this, repo, slotPaths);

    // Code-update boot-state sensors (instance 0 and 1)
    for (uint16_t instance : {ENTITY_INSTANCE_0, ENTITY_INSTANCE_1})
    {
        buildStateSensorPDR(
            this,
            {PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE, instance,
             PLDM_OEM_IBM_BOOT_STATE, 2, 6},
            repo);
    }

    // Firmware-update-state sensor
    buildStateSensorPDR(
        this,
        {PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE, ENTITY_INSTANCE_0,
         PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, 2, 126},
        repo);

    // Verification-state sensor
    buildStateSensorPDR(
        this,
        {PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE, ENTITY_INSTANCE_0,
         PLDM_OEM_IBM_VERIFICATION_STATE, 2, 6},
        repo);

    // Boot-side-rename sensor
    buildStateSensorPDR(
        this,
        {PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE, ENTITY_INSTANCE_0,
         PLDM_OEM_IBM_BOOT_SIDE_RENAME, 2, 6},
        repo);

    // Real SAI sensor
    buildStateSensorPDR(
        this,
        {PLDM_OEM_IBM_ENTITY_REAL_SAI, ENTITY_INSTANCE_1,
         PLDM_STATE_SET_OPERATIONAL_FAULT_STATUS, 2, 6},
        repo);

    // ------------------------------------------------------------------
    // Wire up sensor IDs to the code-update object
    // ------------------------------------------------------------------
    realSAISensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_REAL_SAI, ENTITY_INSTANCE_1,
        1, PLDM_STATE_SET_OPERATIONAL_FAULT_STATUS);

    auto sensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
        ENTITY_INSTANCE_0, 1, PLDM_OEM_IBM_VERIFICATION_STATE);
    codeUpdate->setMarkerLidSensor(sensorId);

    sensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
        ENTITY_INSTANCE_0, 1, PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE);
    codeUpdate->setFirmwareUpdateSensor(sensorId);

    sensorId = findStateSensorId(
        repo.getPdr(), 0, PLDM_OEM_IBM_ENTITY_FIRMWARE_UPDATE,
        ENTITY_INSTANCE_0, 1, PLDM_OEM_IBM_BOOT_SIDE_RENAME);
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
    auto oemPlatformEventMessageResponseHandler = [](mctp_eid_t /*eid*/,
                                                     const pldm_msg* response,
                                                     size_t respMsgLen) {
        uint8_t completionCode{};
        uint8_t status{};
        auto rc = decode_platform_event_message_resp(response, respMsgLen,
                                                     &completionCode, &status);
        if (rc || completionCode)
        {
            error(
                "Failed to decode platform event message response for code update event with response code '{RC}' and completion code '{CC}'",
                "RC", rc, "CC", completionCode);
        }
    };
    auto rc = handler->registerRequest(
        mctp_eid, instanceId, PLDM_PLATFORM, PLDM_PLATFORM_EVENT_MESSAGE,
        std::move(requestMsg),
        std::move(oemPlatformEventMessageResponseHandler));
    if (rc)
    {
        error("Failed to send BIOS attribute change event message ");
    }

    return rc;
}

int encodeEventMsg(uint8_t eventType, const std::vector<uint8_t>& eventDataVec,
                   std::vector<uint8_t>& requestMsg, uint8_t instanceId)
{
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_platform_event_message_req(
        instanceId, 1 /*formatVersion*/, TERMINUS_ID /*tId*/, eventType,
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
    auto instanceIdResult = instanceIdDb.next(mctp_eid);
    if (!instanceIdResult)
    {
        throw pldm::InstanceIdError(instanceIdResult.error());
    }
    auto instanceId = instanceIdResult.value();
    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_PLATFORM_EVENT_MESSAGE_MIN_REQ_BYTES +
        sensorEventDataVec.size());
    auto rc = encodeEventMsg(PLDM_SENSOR_EVENT, sensorEventDataVec, requestMsg,
                             instanceId);
    if (rc != PLDM_SUCCESS)
    {
        error("Failed to encode state sensor event with response code '{RC}'",
              "RC", rc);
        instanceIdDb.free(mctp_eid, instanceId);
        return;
    }
    rc = sendEventToHost(requestMsg, instanceId);
    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to send event to remote terminus with response code '{RC}'",
            "RC", rc);
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
        error("setRequestedApplyTime failed");
        state = CodeUpdateState::FAIL;
    }
    auto sensorId = codeUpdate->getFirmwareUpdateSensor();
    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0, uint8_t(state),
                         uint8_t(CodeUpdateState::END));
}

void pldm::responder::oem_ibm_platform::Handler::updateOemDbusPaths(
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
    /* below logic to replace path 'motherboard/socket/chassis' to
       'motherboard/chassis' or 'motherboard/socket123/chassis' to
       'motherboard/chassis' */
    toFind = "socket";
    size_t pos1 = dbusPath.find(toFind);
    // while loop to detect multiple substring 'socket' in the path
    while (pos1 != std::string::npos)
    {
        size_t pos2 = dbusPath.substr(pos1 + 1).find('/') + 1;
        // Replacing starting from substring to next occurrence of char '/'
        dbusPath.replace(pos1, pos2 + 1, "");
        pos1 = dbusPath.find(toFind);
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
        error(
            "Failure in chassis State transition to Off, unable to set property RequestedPowerTransition, error - {ERROR}",
            "ERROR", e);
    }

    using namespace sdbusplus::match_rules;
    chassisOffMatch = std::make_unique<sdbusplus::match>(
        pldm::utils::DBusHandler::getBus(),
        propertiesChanged("/xyz/openbmc_project/state/chassis0",
                          "xyz.openbmc_project.State.Chassis"),
        [this](sdbusplus::message_t& msg) {
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
                        error(
                            "Failure in setting one-time restore policy, unable to set property PowerRestorePolicy, error - {ERROR}",
                            "ERROR", e);
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
                        error(
                            "Failure in BMC state transition to reboot, unable to set property RequestedBMCTransition , error - {ERROR}",
                            "ERROR", e);
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
    catch (const std::exception&)
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
        bus.call_noreply(resetMethod, dbusTimeout);
    }
    catch (const std::exception& e)
    {
        error("Failed to reset watchdog timer, error - {ERROR}", "ERROR", e);
        return;
    }
}

void pldm::responder::oem_ibm_platform::Handler::disableWatchDogTimer()
{
    setEventReceiverCnt = 0;
    pldm::utils::DBusMapping dbusMapping{
        "/xyz/openbmc_project/watchdog/host0",
        "xyz.openbmc_project.State.Watchdog", "Enabled", "bool"};
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
        error("Failed to disable watchdog timer, error - {ERROR}", "ERROR", e);
    }
}
int pldm::responder::oem_ibm_platform::Handler::checkBMCState()
{
    using BMC = sdbusplus::client::xyz::openbmc_project::state::BMC<>;
    auto bmcPath = sdbusplus::object_path(BMC::namespace_path::value) /
                   BMC::namespace_path::bmc;
    try
    {
        pldm::utils::PropertyValue propertyValue =
            pldm::utils::DBusHandler().getDbusPropertyVariant(
                bmcPath.str.c_str(), "CurrentBMCState", BMC::interface);

        if (std::get<std::string>(propertyValue) !=
            "xyz.openbmc_project.State.BMC.BMCState.Ready")
        {
            error("GetPDR : PLDM stack is not ready for PDR exchange");
            return PLDM_ERROR_NOT_READY;
        }
    }
    catch (const std::exception& e)
    {
        error("Error getting the current BMC state, error - {ERROR}", "ERROR",
              e);
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}

const pldm_pdr_record*
    pldm::responder::oem_ibm_platform::Handler::fetchLastBMCRecord(
        const pldm_pdr* repo)
{
    return pldm_pdr_find_last_in_range(repo, BMC_PDR_START_RANGE,
                                       BMC_PDR_END_RANGE);
}

bool pldm::responder::oem_ibm_platform::Handler::checkRecordHandleInRange(
    const uint32_t& record_handle)
{
    return record_handle >= HOST_PDR_START_RANGE &&
           record_handle <= HOST_PDR_END_RANGE;
}

void Handler::processSetEventReceiver()
{
    this->setEventReceiver();
}

void pldm::responder::oem_ibm_platform::Handler::startStopTimer(bool value)
{
    if (value)
    {
        timer.restart(
            std::chrono::seconds(HEARTBEAT_TIMEOUT + HEARTBEAT_TIMEOUT_DELTA));
    }
    else
    {
        timer.setEnabled(value);
    }
}

void pldm::responder::oem_ibm_platform::Handler::setSurvTimer(uint8_t tid,
                                                              bool value)
{
    if ((hostOff || hostTransitioningToOff || (tid != HYPERVISOR_TID)) &&
        timer.isEnabled())
    {
        startStopTimer(false);
        return;
    }
    if (value)
    {
        startStopTimer(value);
    }
    else if (timer.isEnabled())
    {
        info(
            "Failed to stop surveillance timer while remote terminus status is ‘{HOST_TRANST_OFF}’ with Terminus ID ‘{TID}’ ",
            "HOST_TRANST_OFF", hostTransitioningToOff, "TID", tid);
        startStopTimer(value);
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.setSurvTimer.RecvSurveillancePingFail");
    }
}

void pldm::responder::oem_ibm_platform::Handler::handleBootTypesAtPowerOn()
{
    PendingAttributesList biosAttrList;
    auto bootInitiator =
        getBiosAttrValue<std::string>("pvm_boot_initiator_current")
            .value_or("");
    std::string restartCause;
    if (((bootInitiator != "HMC") || (bootInitiator != "Host")) &&
        !bootInitiator.empty())
    {
        try
        {
            restartCause =
                pldm::utils::DBusHandler().getDbusProperty<std::string>(
                    "/xyz/openbmc_project/state/host0",
                    HostState::property_names::restart_cause,
                    sdbusplus::common::xyz::openbmc_project::state::Host::
                        interface);
            setBootTypesBiosAttr(restartCause);
        }
        catch (const std::exception& e)
        {
            error(
                "Failed to set the D-bus property for the Host restart reason ERROR={ERR}",
                "ERR", e);
        }
    }
}

void pldm::responder::oem_ibm_platform::Handler::setBootTypesBiosAttr(
    const std::string& restartCause)
{
    PendingAttributesList biosAttrList;
    if (restartCause ==
        "xyz.openbmc_project.State.Host.RestartCause.ScheduledPowerOn")
    {
        biosAttrList.emplace_back(std::make_pair(
            "pvm_boot_initiator", std::make_tuple(EnumAttribute, "Host")));
        setBiosAttr(biosAttrList);
    }
    else if (
        (restartCause ==
         "xyz.openbmc_project.State.Host.RestartCause.PowerPolicyAlwaysOn") ||
        (restartCause ==
         "xyz.openbmc_project.State.Host.RestartCause.PowerPolicyPreviousState"))
    {
        biosAttrList.emplace_back(std::make_pair(
            "pvm_boot_initiator", std::make_tuple(EnumAttribute, "Auto")));
        setBiosAttr(biosAttrList);
    }
    else if (restartCause ==
             "xyz.openbmc_project.State.Host.RestartCause.HostCrash")
    {
        biosAttrList.emplace_back(std::make_pair(
            "pvm_boot_initiator", std::make_tuple(EnumAttribute, "Auto")));
        biosAttrList.emplace_back(std::make_pair(
            "pvm_boot_type", std::make_tuple(EnumAttribute, "ReIPL")));
        setBiosAttr(biosAttrList);
    }
}

void pldm::responder::oem_ibm_platform::Handler::handleBootTypesAtChassisOff()
{
    PendingAttributesList biosAttrList;
    auto bootInitiator =
        getBiosAttrValue<std::string>("pvm_boot_initiator").value_or("");
    auto bootType = getBiosAttrValue<std::string>("pvm_boot_type").value_or("");
    if (bootInitiator.empty() || bootType.empty())
    {
        error(
            "ERROR in fetching the pvm_boot_initiator and pvm_boot_type BIOS attribute values");
        return;
    }
    else if (bootInitiator != "Host")
    {
        biosAttrList.emplace_back(std::make_pair(
            "pvm_boot_initiator", std::make_tuple(EnumAttribute, "User")));
        biosAttrList.emplace_back(std::make_pair(
            "pvm_boot_type", std::make_tuple(EnumAttribute, "IPL")));
        setBiosAttr(biosAttrList);
    }
}

void pldm::responder::oem_ibm_platform::Handler::turnOffRealSAIEffecter()
{
    try
    {
        pldm::utils::DBusMapping dbusPartitionMapping{
            "/xyz/openbmc_project/led/groups/partition_system_attention_indicator",
            "xyz.openbmc_project.Led.Group", "Asserted", "bool"};
        pldm::utils::DBusHandler().setDbusProperty(dbusPartitionMapping, false);
    }
    catch (const std::exception& e)
    {
        error("Turn off of partition SAI effecter failed with "
              "error:{ERR_EXCEP}",
              "ERR_EXCEP", e);
    }
    try
    {
        pldm::utils::DBusMapping dbusPlatformMapping{
            "/xyz/openbmc_project/led/groups/platform_system_attention_indicator",
            "xyz.openbmc_project.Led.Group", "Asserted", "bool"};
        pldm::utils::DBusHandler().setDbusProperty(dbusPlatformMapping, false);
    }
    catch (const std::exception& e)
    {
        error("Turn off of platform SAI effecter failed with "
              "error:{ERR_EXCEP}",
              "ERR_EXCEP", e);
    }
}

uint8_t pldm::responder::oem_ibm_platform::Handler::fetchRealSAIStatus()
{
    try
    {
        auto isPartitionSAIOn = pldm::utils::DBusHandler().getDbusProperty<bool>(
            "/xyz/openbmc_project/led/groups/partition_system_attention_indicator",
            "Asserted", "xyz.openbmc_project.Led.Group");
        auto isPlatformSAIOn = pldm::utils::DBusHandler().getDbusProperty<bool>(
            "/xyz/openbmc_project/led/groups/platform_system_attention_indicator",
            "Asserted", "xyz.openbmc_project.Led.Group");

        if (isPartitionSAIOn || isPlatformSAIOn)
        {
            return PLDM_SENSOR_WARNING;
        }
    }
    catch (const std::exception& e)
    {
        error("Fetching of Real SAI sensor status failed with "
              "error:{ERR_EXCEP}",
              "ERR_EXCEP", e);
    }
    return PLDM_SENSOR_NORMAL;
}

void pldm::responder::oem_ibm_platform::Handler::processSAIUpdate()
{
    auto realSAIState = fetchRealSAIStatus();
    sendStateSensorEvent(realSAISensorId, PLDM_STATE_SENSOR_STATE, 0,
                         uint8_t(realSAIState), uint8_t(PLDM_SENSOR_UNKNOWN));
}

} // namespace oem_ibm_platform
} // namespace responder
} // namespace pldm
