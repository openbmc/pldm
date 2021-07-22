#include "collect_slot_vpd.hpp"

#include "oem_ibm_handler.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

namespace pldm
{

namespace responder
{
using namespace oem_ibm_platform;
void SlotHandler::timeOutHandler()
{
    std::cerr << "Timer expired waiting for Event from Inventory" << std::endl;

    // Disable the timer
    timer.setEnabled(false);

    // obtain the sensor Id
    auto sensorId = pldm::utils::findStateSensorId(
        pdrRepo, 0, PLDM_ENTITY_SLOT,
        current_on_going_slot_entity.entity_instance_num,
        current_on_going_slot_entity.entity_container_id,
        PLDM_OEM_IBM_SLOT_ENABLE_SENSOR_STATE);

    // send the sensor event to host with error state
    sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                         uint8_t(SLOT_STATE_ERROR), uint8_t(SLOT_STATE_UNKOWN));
}

void SlotHandler::enableSlot(uint16_t effecterId,
                             const AssociatedEntityMap& fruAssociationMap,
                             uint8_t stateFileValue)

{
    std::cerr << (unsigned)effecterId << std::endl;
    const pldm_entity entity = getEntityIDfromEffecterID(effecterId);

    for (const auto& [key, value] : fruAssociationMap)
    {
        if (entity.entity_instance_num == value.entity_instance_num &&
            entity.entity_type == value.entity_type &&
            entity.entity_container_id == value.entity_container_id)
        {
            std::cerr << key << std::endl;
            std::cerr << (unsigned)value.entity_type << std::endl;
            std::cerr << (unsigned)value.entity_instance_num << std::endl;
            this->current_on_going_slot_entity.entity_type = value.entity_type;
            this->current_on_going_slot_entity.entity_instance_num =
                value.entity_instance_num;
            this->current_on_going_slot_entity.entity_container_id =
                value.entity_container_id;
            processSlotOperations(key, value, stateFileValue);
        }
    }
}

void SlotHandler::processSlotOperations(const std::string& slotObjectPath,
                                        const pldm_entity& entity,
                                        uint8_t stateFiledValue)
{

    std::string adapterObjPath;
    try
    {
        // get the adapter dbus object path from the slot dbus object path
        adapterObjPath = getAdapterObjPath(slotObjectPath).value();
    }
    catch (const std::bad_optional_access& e)
    {
        std::cerr << e.what() << '\n';
        return;
    }

    // create a presence match for the adpter present property
    createPresenceMatch(adapterObjPath, entity, stateFiledValue);

    // call the VPD Manager to collect/remove VPD objects
    callVPDManager(adapterObjPath, stateFiledValue);

    // start the 1 min timer
    timer.restart(std::chrono::seconds(60));
}

void SlotHandler::callVPDManager(const std::string& adapterObjPath,
                                 uint8_t stateFiledValue)
{
    static constexpr auto VPDObjPath = "/com/ibm/VPD/Manager";
    static constexpr auto VPDInterface = "com.ibm.VPD.Manager";
    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        auto service =
            pldm::utils::DBusHandler().getService(VPDObjPath, VPDInterface);
        if (stateFiledValue == uint8_t(ADD))
        {
            auto method = bus.new_method_call(service.c_str(), VPDObjPath,
                                              VPDInterface, "CollectFRUVPD");
            method.append(
                static_cast<sdbusplus::message::object_path>(adapterObjPath));
            bus.call_noreply(method);
        }
        else if (stateFiledValue == uint8_t(REMOVE) ||
                 stateFiledValue == uint8_t(REPLACE))
        {
            auto method = bus.new_method_call(service.c_str(), VPDObjPath,
                                              VPDInterface, "deleteFRUVPD");
            method.append(
                static_cast<sdbusplus::message::object_path>(adapterObjPath));
            bus.call_noreply(method);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to VPD Manager , Operation ="
                  << (unsigned)stateFiledValue << ", ERROR=" << e.what()
                  << "\n";
    }
}

std::optional<std::string>
    SlotHandler::getAdapterObjPath(const std::string& slotObjPath)
{
    static constexpr auto searchpath = "/xyz/openbmc_project/inventory";
    int depth = 0;
    std::vector<std::string> pcieAdapterInterface = {
        "xyz.openbmc_project.Inventory.Item.PCIeDevice"};
    pldm::utils::GetSubTreeResponse response =
        pldm::utils::DBusHandler().getSubtree(searchpath, depth,
                                              pcieAdapterInterface);
    for (const auto& [objPath, serviceMap] : response)
    {
        // An adapter is a child of a PCIe Slot Object
        if (objPath.find(slotObjPath) != std::string::npos)
        {
            // Found the Adapter under the slot
            return objPath;
        }
    }
    return std::nullopt;
}

void SlotHandler::createPresenceMatch(const std::string& adapterObjectPath,
                                      const pldm_entity& entity,
                                      uint8_t stateFieldValue)
{

    std::cerr << "Create presence match for present property under"
              << adapterObjectPath << std::endl;
    fruPresenceMatch = std::make_unique<sdbusplus::bus::match::match>(
        pldm::utils::DBusHandler::getBus(),
        propertiesChanged(adapterObjectPath,
                          "xyz.openbmc_project.Inventory.Item"),
        [this, adapterObjectPath, stateFieldValue,
         entity](sdbusplus::message::message& msg) {
            pldm::utils::DbusChangedProps props{};
            std::string intf;
            msg.read(intf, props);
            const auto itr = props.find("Present");
            if (itr != props.end())
            {
                bool value = std::get<bool>(itr->second);
                // Present Property is found
                this->processPropertyChangeFromVPD(value, adapterObjectPath,
                                                   stateFieldValue, entity);
            }
        });
}

void SlotHandler::processPropertyChangeFromVPD(
    bool presentValue, const std::string& /*adapterObjectPath*/,
    uint8_t stateFiledvalue, const pldm_entity& entity)
{
    // irrespective of true->false or false->true change, we should stop the
    // timer
    timer.setEnabled(false);

    // remove the prence match so that it does not monitor the change
    // even after we captured the property changed signal
    fruPresenceMatch = nullptr;

    // obtain the sensor id attached with this slot
    auto sensorId = pldm::utils::findStateSensorId(
        pdrRepo, 0, PLDM_ENTITY_SLOT, entity.entity_instance_num,
        entity.entity_container_id, PLDM_OEM_IBM_SLOT_ENABLE_SENSOR_STATE);

    uint8_t sensorOpState = uint8_t(SLOT_STATE_UNKOWN);
    if (presentValue)
    {
        sensorOpState = uint8_t(SLOT_STATE_ENABLED);
    }
    else
    {
        if (stateFiledvalue == uint8_t(REPLACE))
        {
            sensorOpState = uint8_t(SLOT_STATE_UNKOWN);
        }
        else
        {
            sensorOpState = uint8_t(SLOT_STATE_DISABLED);
        }
    }
    // set the sensor state based on the stateFieldValue
    this->sendStateSensorEvent(sensorId, PLDM_STATE_SENSOR_STATE, 0,
                               sensorOpState, uint8_t(SLOT_STATE_UNKOWN));
}

pldm_entity SlotHandler::getEntityIDfromEffecterID(uint16_t effecterId)
{
    pldm_entity parentfruentity{};
    uint8_t* pdrData = nullptr;
    uint32_t pdrSize{};
    const pldm_pdr_record* record{};
    do
    {
        record = pldm_pdr_find_record_by_type(pdrRepo, PLDM_STATE_EFFECTER_PDR,
                                              record, &pdrData, &pdrSize);
        if (record && (true ^ pldm_pdr_record_is_remote(record)))
        {
            auto pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrData);
            auto compositeEffecterCount = pdr->composite_effecter_count;
            auto possible_states_start = pdr->possible_states;

            for (auto effecters = 0x00; effecters < compositeEffecterCount;
                 effecters++)
            {

                auto possibleStates =
                    reinterpret_cast<state_effecter_possible_states*>(
                        possible_states_start);

                if (possibleStates->state_set_id ==
                        PLDM_OEM_IBM_SLOT_ENABLE_EFFECTER_STATE &&
                    effecterId == pdr->effecter_id)
                {
                    parentfruentity.entity_type = pdr->entity_type;
                    parentfruentity.entity_instance_num = pdr->entity_instance;
                    parentfruentity.entity_container_id = pdr->container_id;

                    return parentfruentity;
                }
            }
        }
    } while (record);

    return parentfruentity;
}

uint8_t SlotHandler::fetchSlotSensorState(const std::string& slotObjectPath)
{
    std::string adapterObjPath;
    uint8_t sensorOpState = uint8_t(SLOT_STATE_UNKOWN);

    try
    {
        // get the adapter dbus object path from the slot dbus object path
        adapterObjPath = getAdapterObjPath(slotObjectPath).value();
    }
    catch (const std::bad_optional_access& e)
    {
        std::cerr << e.what() << '\n';
        return uint8_t(SLOT_STATE_UNKOWN);
    }

    if (fetchSensorStateFromDbus(adapterObjPath))
    {
        sensorOpState = uint8_t(SLOT_STATE_ENABLED);
    }
    else
    {
        sensorOpState = uint8_t(SLOT_STATE_DISABLED);
    }
    return sensorOpState;
}

void SlotHandler::setOemPlatformHandler(
    pldm::responder::oem_platform::Handler* handler)
{
    oemPlatformHandler = handler;
}

void SlotHandler::sendStateSensorEvent(
    uint16_t sensorId, enum sensor_event_class_states sensorEventClass,
    uint8_t sensorOffset, uint8_t eventState, uint8_t prevEventState)
{
    pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
        dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
            oemPlatformHandler);
    oemIbmPlatformHandler->sendStateSensorEvent(
        sensorId, sensorEventClass, sensorOffset, eventState, prevEventState);
}

bool SlotHandler::fetchSensorStateFromDbus(const std::string& adapterObjectPath)
{
    std::cerr << "fetch sensor state from dbus" << std::endl;

    std::variant<bool> presentProperty{};
    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        auto service = pldm::utils::DBusHandler().getService(
            adapterObjectPath.c_str(), ItemInterface);
        auto method =
            bus.new_method_call(service.c_str(), adapterObjectPath.c_str(),
                                FreedesktopInterface, GetMethod);
        method.append(ItemInterface, PresentProperty);
        auto reply = bus.call(method);
        reply.read(presentProperty);
        return std::get<bool>(presentProperty);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to Inventory manager, ERROR="
                  << e.what() << "\n";
    }

    return false;
}

} // namespace responder
} // namespace pldm
