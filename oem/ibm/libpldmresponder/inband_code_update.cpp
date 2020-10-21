#include "inband_code_update.hpp"

#include "libpldmresponder/pdr.hpp"
#include "oem_ibm_handler.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <arpa/inet.h>

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Dump/NewDump/server.hpp>

#include <exception>
#include <fstream>
#include <regex>
namespace pldm
{

namespace responder
{

/** @brief Directory where the lid files without a header are stored */
auto lidDirPath = fs::path(LID_STAGING_DIR) / "lid";

/** @brief Directory where the image files are stored as they are built */
auto imageDirPath = fs::path(LID_STAGING_DIR) / "image";

/** @brief Directory where the code update tarball files are stored */
auto updateDirPath = fs::path(LID_STAGING_DIR) / "update";

/** @brief The file name of the code update tarball */
constexpr auto tarImageName = "image.tar";

/** @brief The file name of the hostfw image */
constexpr auto hostfwImageName = "image-host-fw";

/** @brief The path to the code update tarball file */
auto tarImagePath = fs::path(imageDirPath) / tarImageName;

/** @brief The path to the hostfw image */
auto hostfwImagePath = fs::path(imageDirPath) / hostfwImageName;

/** @brief The path to the tarball file expected by the phosphor software
 *         manager */
auto updateImagePath = fs::path("/tmp/images") / tarImageName;

std::string CodeUpdate::fetchCurrentBootSide()
{
    return currBootSide;
}

std::string CodeUpdate::fetchNextBootSide()
{
    return nextBootSide;
}

int CodeUpdate::setCurrentBootSide(const std::string& currSide)
{
    currBootSide = currSide;
    return PLDM_SUCCESS;
}

int CodeUpdate::setNextBootSide(const std::string& nextSide)
{
    int rc = PLDM_SUCCESS;

    nextBootSide = nextSide;
    static constexpr auto PROP_INTF =
        "xyz.openbmc_project.Software.RedundancyPriority";
    std::string obj_path{};
    if (nextBootSide == currBootSide)
    {
        obj_path = runningVersion;
    }
    else
    {
        obj_path = nonRunningVersion;
    }
    if (obj_path.empty())
    {
        std::cerr << "no nonRunningVersion present \n";
        return PLDM_PLATFORM_INVALID_STATE_VALUE;
    }

    pldm::utils::DBusMapping dbusMapping{obj_path, PROP_INTF, "Priority",
                                         "uint8_t"};
    uint8_t val = 0;
    pldm::utils::PropertyValue value = static_cast<uint8_t>(val);
    try
    {
        dBusIntf->setDbusProperty(dbusMapping, value);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to set the next boot side to " << obj_path.c_str()
                  << " ERROR=" << e.what() << "\n";
        rc = PLDM_ERROR;
    }
    return rc;
}

int CodeUpdate::setRequestedApplyTime()
{
    int rc = PLDM_SUCCESS;
    static constexpr auto SETTINGS_SERVICE = "xyz.openbmc_project.Settings";
    static constexpr auto APPLY_TIME_OBJ_PATH =
        "/xyz/openbmc_project/software/apply_time";
    static constexpr auto APPLY_TIME_INTF =
        "xyz.openbmc_project.Software.ApplyTime";
    static constexpr auto PROP_INTF = "org.freedesktop.DBus.Properties";
    auto& bus = dBusIntf->getBus();
    pldm::utils::PropertyValue value =
        "xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.OnReset";
    try
    {
        auto method = bus.new_method_call(SETTINGS_SERVICE, APPLY_TIME_OBJ_PATH,
                                          PROP_INTF, "Set");
        method.append(APPLY_TIME_INTF, "RequestedApplyTime", value);
        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed To set RequestedApplyTime property "
                  << "ERROR=" << e.what() << std::endl;
        rc = PLDM_ERROR;
    }
    return rc;
}

int CodeUpdate::setRequestedActivation(CodeUpdate* codeUpdate)
{
    int rc = PLDM_SUCCESS;
    std::string img = codeUpdate->fetchnewImageId();
    static constexpr auto UPDATE_SERVICE =
        "xyz.openbmc_project.Software.BMC.Updater";
    const std::string objPath(img);
    static constexpr auto REQUESTED_ACTIVATION_INTF =
        "xyz.openbmc_project.Software.Activation";
    static constexpr auto PROP_INTF = "org.freedesktop.DBus.Properties";
    auto& bus = dBusIntf->getBus();
    pldm::utils::PropertyValue value =
        "xyz.openbmc_project.Software.Activation.RequestedActivations.Active";
    try
    {
        auto method = bus.new_method_call(UPDATE_SERVICE, objPath.c_str(),
                                          PROP_INTF, "Set");
        method.append(REQUESTED_ACTIVATION_INTF, "RequestedActivation", value);

        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed To set RequestedActivation property"
                  << "ERROR=" << e.what() << std::endl;
        rc = PLDM_ERROR;
    }
    return rc;
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
        std::variant<std::vector<std::string>> paths;

        auto reply = bus.call(method);
        reply.read(paths);

        runningVersion = std::get<std::vector<std::string>>(paths)[0];

        auto method1 = bus.new_method_call(MAPPER_SERVICE, ACTIVE_OBJ_PATH,
                                           PROP_INTF, "Get");
        method1.append("xyz.openbmc_project.Association", "endpoints");

        auto reply1 = bus.call(method1);
        reply1.read(paths);
        for (const auto& path : std::get<std::vector<std::string>>(paths))
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

    using namespace sdbusplus::bus::match::rules;
    static constexpr auto PROP_CHANGE_INTF =
        "xyz.openbmc_project.Software.RedundancyPriority";
    captureNextBootSideChange.push_back(
        std::make_unique<sdbusplus::bus::match::match>(
            pldm::utils::DBusHandler::getBus(),
            propertiesChanged(runningVersion, PROP_CHANGE_INTF),
            [this](sdbusplus::message::message& msg) {
                DbusChangedProps props;
                std::string iface;
                msg.read(iface, props);
                processPriorityChangeNotification(props);
            }));
    fwUpdateMatcher = std::make_unique<sdbusplus::bus::match::match>(
        pldm::utils::DBusHandler::getBus(),
        "interface='org.freedesktop.DBus.ObjectManager',type='signal',"
        "member='InterfacesAdded',path='/xyz/openbmc_project/software'",
        [this](sdbusplus::message::message& msg) {
            DBusInterfaceAdded interfaces;
            sdbusplus::message::object_path path;
            msg.read(path, interfaces);
            for (auto& interface : interfaces)
            {
                if (interface.first ==
                    "xyz.openbmc_project.Software.Activation")
                {
                    newImageId = path.str;
                    break;
                }
            }
        });
}

void CodeUpdate::processPriorityChangeNotification(
    const DbusChangedProps& chProperties)
{
    static constexpr auto propName = "Priority";
    const auto it = chProperties.find(propName);
    if (it == chProperties.end())
    {
        return;
    }
    uint8_t newVal = std::get<uint8_t>(it->second);
    nextBootSide = (newVal == 0) ? currBootSide
                                 : ((currBootSide == Tside) ? Pside : Tside);
}

uint8_t fetchBootSide(uint16_t entityInstance, CodeUpdate* codeUpdate)
{
    uint8_t sensorOpState = T_SIDE;
    if (entityInstance == 0)
    {
        auto currSide = codeUpdate->fetchCurrentBootSide();
        if (currSide == Pside)
        {
            sensorOpState = P_SIDE;
        }
    }
    else if (entityInstance == 1)
    {
        auto nextSide = codeUpdate->fetchNextBootSide();
        if (nextSide == Pside)
        {
            sensorOpState = P_SIDE;
        }
    }
    else
    {
        sensorOpState = PLDM_SENSOR_UNKNOWN;
    }

    return sensorOpState;
}

int setBootSide(uint16_t entityInstance, uint8_t currState,
                const std::vector<set_effecter_state_field>& stateField,
                CodeUpdate* codeUpdate)
{
    int rc = PLDM_SUCCESS;
    auto side = (stateField[currState].effecter_state == P_SIDE) ? "P" : "T";

    if (entityInstance == 0)
    {
        rc = codeUpdate->setCurrentBootSide(side);
    }
    else if (entityInstance == 1)
    {
        rc = codeUpdate->setNextBootSide(side);
    }
    else
    {
        rc = PLDM_PLATFORM_INVALID_STATE_VALUE;
    }
    return rc;
}

void generateStateEffecterOEMPDR(platform::Handler* platformHandler,
                                 uint16_t entityInstance, uint16_t stateSetID,
                                 pdr_utils::Repo& repo)
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
    pdr->effecter_id = platformHandler->getNextEffecterId();
    pdr->entity_type = PLDM_VIRTUAL_MACHINE_MANAGER_ENTITY;
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
    if (stateSetID == oem_ibm_platform::PLDM_OEM_IBM_BOOT_STATE)
        state->states[0].byte = 6;
    else if (stateSetID == oem_ibm_platform::PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void generateStateSensorOEMPDR(platform::Handler* platformHandler,
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

    pdr->hdr.record_handle = 0;
    pdr->hdr.version = 1;
    pdr->hdr.type = PLDM_STATE_SENSOR_PDR;
    pdr->hdr.record_change_num = 0;
    pdr->hdr.length = sizeof(pldm_state_sensor_pdr) - sizeof(pldm_pdr_hdr);
    pdr->terminus_handle = pdr::BmcPldmTerminusHandle;
    pdr->sensor_id = platformHandler->getNextSensorId();
    pdr->entity_type = entityType;
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
    if ((stateSetID == oem_ibm_platform::PLDM_OEM_IBM_BOOT_STATE) ||
        (stateSetID == oem_ibm_platform::PLDM_OEM_IBM_VERIFICATION_STATE))
        state->states[0].byte = 6;
    else if (stateSetID == oem_ibm_platform::PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE)
        state->states[0].byte = 126;
    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    pdrEntry.data = entry.data();
    pdrEntry.size = pdrSize;
    repo.addRecord(pdrEntry);
}

void buildAllCodeUpdateEffecterPDR(platform::Handler* platformHandler,
                                   pdr_utils::Repo& repo)
{
    generateStateEffecterOEMPDR(
        platformHandler, oem_ibm_platform::ENTITY_INSTANCE_0,
        oem_ibm_platform::PLDM_OEM_IBM_BOOT_STATE, repo);
    generateStateEffecterOEMPDR(
        platformHandler, oem_ibm_platform::ENTITY_INSTANCE_1,
        oem_ibm_platform::PLDM_OEM_IBM_BOOT_STATE, repo);
    generateStateEffecterOEMPDR(
        platformHandler, oem_ibm_platform::ENTITY_INSTANCE_0,
        oem_ibm_platform::PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, repo);
}

void buildAllCodeUpdateSensorPDR(platform::Handler* platformHandler,
                                 pdr_utils::Repo& repo)
{
    generateStateSensorOEMPDR(platformHandler,
                              PLDM_VIRTUAL_MACHINE_MANAGER_ENTITY,
                              oem_ibm_platform::ENTITY_INSTANCE_0,
                              oem_ibm_platform::PLDM_OEM_IBM_BOOT_STATE, repo);
    generateStateSensorOEMPDR(platformHandler,
                              PLDM_VIRTUAL_MACHINE_MANAGER_ENTITY,
                              oem_ibm_platform::ENTITY_INSTANCE_1,
                              oem_ibm_platform::PLDM_OEM_IBM_BOOT_STATE, repo);
    generateStateSensorOEMPDR(
        platformHandler, PLDM_VIRTUAL_MACHINE_MANAGER_ENTITY,
        oem_ibm_platform::ENTITY_INSTANCE_0,
        oem_ibm_platform::PLDM_OEM_IBM_FIRMWARE_UPDATE_STATE, repo);

    generateStateSensorOEMPDR(platformHandler, PLDM_SYSTEM_FIRMWARE,
                              oem_ibm_platform::ENTITY_INSTANCE_0,
                              oem_ibm_platform::PLDM_OEM_IBM_VERIFICATION_STATE,
                              repo);
}

template <typename... T>
int executeCmd(T const&... t)
{
    std::stringstream cmd;
    ((cmd << t << " "), ...) << std::endl;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    int rc = pclose(pipe);
    if (WEXITSTATUS(rc))
    {
        std::cerr << "Error executing: ";
        ((std::cerr << " " << t), ...);
        std::cerr << "\n";
        return -1;
    }

    return 0;
}

int processCodeUpdateLid(const std::string& filePath)
{
    struct lidHeader
    {
        uint16_t magicNumber;
        uint16_t headerVersion;
        uint32_t lidNumber;
        uint32_t lidDate;
        uint16_t lidTime;
        uint16_t lidClass;
        uint32_t lidCrc;
        uint32_t lidSize;
        uint32_t headerSize;
    };
    lidHeader header;

    std::ifstream ifs(filePath, std::ios::in | std::ios::binary);
    if (!ifs)
    {
        std::cerr << "ifstream open error: " << filePath << "\n";
        return PLDM_ERROR;
    }
    ifs.seekg(0);
    ifs.read(reinterpret_cast<char*>(&header), sizeof(header));

    // File size should be the value of lid size minus the header size
    auto fileSize = fs::file_size(filePath);
    fileSize -= htonl(header.headerSize);
    if (fileSize < htonl(header.lidSize))
    {
        // File is not completely written yet
        ifs.close();
        return PLDM_SUCCESS;
    }

    constexpr auto magicNumber = 0x0222;
    if (htons(header.magicNumber) != magicNumber)
    {
        std::cerr << "Invalid magic number: " << filePath << "\n";
        ifs.close();
        return PLDM_ERROR;
    }

    fs::create_directories(imageDirPath);
    fs::create_directories(lidDirPath);

    constexpr auto bmcClass = 0x2000;
    if (htons(header.lidClass) == bmcClass)
    {
        // Skip the header and concatenate the BMC LIDs into a tar file
        std::ofstream ofs(tarImagePath,
                          std::ios::out | std::ios::binary | std::ios::app);
        ifs.seekg(htonl(header.headerSize));
        ofs << ifs.rdbuf();
        ofs.flush();
        ofs.close();
    }
    else
    {
        std::stringstream lidFileName;
        lidFileName << std::hex << htonl(header.lidNumber) << ".lid";
        auto lidNoHeaderPath = fs::path(lidDirPath) / lidFileName.str();
        std::ofstream ofs(lidNoHeaderPath,
                          std::ios::out | std::ios::binary | std::ios::trunc);
        ifs.seekg(htonl(header.headerSize));
        ofs << ifs.rdbuf();
        ofs.flush();
        ofs.close();
    }

    ifs.close();
    fs::remove(filePath);
    return PLDM_SUCCESS;
}

int assembleCodeUpdateImage()
{
    // Create the hostfw squashfs image from the LID files without header
    auto rc = executeCmd("/usr/sbin/mksquashfs", lidDirPath.c_str(),
                         hostfwImagePath.c_str(), "-all-root", "-no-recovery");
    if (rc < 0)
    {
        return PLDM_ERROR;
    }

    fs::create_directories(updateDirPath);

    // Extract the BMC tarball content
    rc = executeCmd("/bin/tar", "-xf", tarImagePath.c_str(), "-C",
                    updateDirPath);
    if (rc < 0)
    {
        return PLDM_ERROR;
    }

    // Add the hostfw image to the directory where the contents were extracted
    fs::copy_file(hostfwImagePath, fs::path(updateDirPath) / hostfwImageName,
                  fs::copy_options::overwrite_existing);

    // Remove the tarball file, then re-generate it with so that the hostfw
    // image becomes part of the tarball
    fs::remove(tarImagePath);
    rc = executeCmd("/bin/tar", "-cf", tarImagePath, ".", "-C", updateDirPath);
    if (rc < 0)
    {
        return PLDM_ERROR;
    }

    // Copy the tarball to the update directory to trigger the phosphor software
    // manager to create a version interface
    fs::copy_file(tarImagePath, updateImagePath,
                  fs::copy_options::overwrite_existing);

    // Cleanup
    fs::remove_all(updateDirPath);
    fs::remove_all(lidDirPath);
    fs::remove_all(imageDirPath);

    return PLDM_SUCCESS;
}

} // namespace responder

} // namespace pldm
