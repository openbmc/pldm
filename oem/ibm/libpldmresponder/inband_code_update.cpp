#include "inband_code_update.hpp"
#include "xyz/openbmc_project/Common/error.hpp"
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Dump/NewDump/server.hpp>
#include "libpldmresponder/pdr.hpp"
//#include "oem_ibm_handler.hpp"
#include <exception>

namespace pldm
{

namespace responder
{
constexpr uint16_t BOOT_SIDE_ID = 32769;
constexpr uint16_t FW_UPDATE_ID = 32768;
constexpr uint16_t ENTITY_INSTANCE_0 = 0;
constexpr uint16_t ENTITY_INSTANCE_1 = 1;


std::string CodeUpdate::fetchCurrentBootSide()
{
    return currBootSide;
}

std::string CodeUpdate::fetchNextBootSide()
{
    return nextBootSide;
}

int CodeUpdate::setCurrentBootSide(std::string currSide)
{
    currBootSide = currSide;
    return PLDM_SUCCESS;
}

int CodeUpdate::setNextBootSide(std::string nextSide)
{
    nextBootSide = nextSide;
    // change the priority property of version
    return PLDM_SUCCESS;
}

void CodeUpdate::setVersions()
{
    static constexpr auto MAPPER_SERVICE = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto FUNCTIONAL_OBJ_PATH =
        "/xyz/openbmc_project/software/functional";
    static constexpr auto ACTIVE_OBJ_PATH =
        "/xyz/openbmc_project/software/active";
    static constexpr auto PROP_INTF = "org.freedesktop.DBus.Properties";

    auto& bus = dBusIntf->getBus();

    try
    {
        auto method = bus.new_method_call(MAPPER_SERVICE, FUNCTIONAL_OBJ_PATH,
                                          PROP_INTF, "Get");
        method.append("xyz.openbmc_project.Association", "endpoints");

        auto reply = bus.call(method);
        reply.read(runningVersion);

        std::vector<std::string> paths;
        auto method1 = bus.new_method_call(MAPPER_SERVICE, ACTIVE_OBJ_PATH,
                                           PROP_INTF, "Get");
        method1.append("xyz.openbmc_project.Association", "endpoints");

        auto reply1 = bus.call(method1);
        reply.read(paths);
        for (const auto& path : paths)
        {
            if (path != runningVersion)
            {
                nonRunningVersion = path;
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to Object Mapper "
                     "Association, ERROR="
                  << e.what() << "\n";
        return;
    }
}

/*void generateStateEffecterOEMPDR(platform::Handler* platformHandler,
                                 uint16_t entityInstance, uint16_t stateSetID,
                                 pdr_utils::RepoInterface& repo)
{

    size_t pdrSize = 0;
    pdrSize = sizeof(pldm_state_effecter_pdr) +
              sizeof(state_effecter_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(entry.data());

    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_effecter_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    if ( oemPlatformHandler != nullptr)
    {
        pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
                dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
                    oemPlatformHandler);
        pdr->effecter_id = oemIbmPlatformHandler->getNextEffecterId();
    }
    //pdr->effecter_id = platformHandler->getNextEffecterId();
    //std::cout<< platformHandler->getNextEffecterId() << std::endl;
    pdr->entity_type = 33;
    pdr->entity_instance = entityInstance;
    pdr->container_id = 0;
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
    if (stateSetID == BOOT_SIDE_ID)
        state->states[0].byte = 6;
    else if (stateSetID == FW_UPDATE_ID)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}*/


void generateStateSensorOEMPDR(uint16_t entityInstance, 
                               uint16_t stateSetID,
                               pdr_utils::RepoInterface& repo)
{
    size_t pdrSize = 0;
    pdrSize =
        sizeof(pldm_state_sensor_pdr) + sizeof(state_sensor_possible_states);
    std::vector<uint8_t> entry{};
    entry.resize(pdrSize);
    pldm_state_sensor_pdr* pdr =
        reinterpret_cast<pldm_state_sensor_pdr*>(entry.data());

    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_SENSOR_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_sensor_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    //pdr->sensor_id = nextSensorId;
    pdr->entity_type = 33;
    pdr->entity_instance = entityInstance;
    pdr->container_id = 0;
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
    if (stateSetID == BOOT_SIDE_ID)
        state->states[0].byte = 6;
    else if (stateSetID == FW_UPDATE_ID)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

//void buildAllCodeUpdateEffecterPDR(uint16_t nextEffecterId,
  //                                 pdr_utils::RepoInterface& repo)
/*void buildAllCodeUpdateEffecterPDR(platform::Handler* platformHandler,
                                   pdr_utils::RepoInterface& repo)
{
    //generateStateEffecterOEMPDR(platformHandler,ENTITY_INSTANCE_0,BOOT_SIDE_ID,
                                repo);
    //generateStateEffecterOEMPDR(platformHandler,ENTITY_INSTANCE_1,BOOT_SIDE_ID, 
                                repo);
    //generateStateEffecterOEMPDR(platformHandler,ENTITY_INSTANCE_0,FW_UPDATE_ID, 
                                repo);

}*/

void buildAllCodeUpdateSensorPDR(pdr_utils::RepoInterface& repo)
{
    generateStateSensorOEMPDR(ENTITY_INSTANCE_0,BOOT_SIDE_ID, 
                              repo);
    generateStateSensorOEMPDR(ENTITY_INSTANCE_1,BOOT_SIDE_ID, 
                              repo);
    generateStateSensorOEMPDR(ENTITY_INSTANCE_0,FW_UPDATE_ID, 
                              repo);

}

} // namespace responder

} // namespace pldm
