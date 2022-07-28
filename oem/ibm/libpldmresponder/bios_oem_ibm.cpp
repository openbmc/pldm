#include "bios_oem_ibm.hpp"

namespace pldm
{
namespace responder
{
namespace oem_ibm_bios
{
void pldm::responder::oem_ibm_bios::Handler::setBiosHandler(
    pldm::responder::bios::Handler* handler)
{
    biosHandler = handler;
}

/** @brief Method to get the system type information
 *
 *  @return[std::filesystem::path] - the system type information
 */
std::filesystem::path pldm::responder::oem_ibm_bios::Handler::getPlatformName()
{
    if (!systemType.empty())
    {
        return fs::path{systemType};
    }

    static constexpr auto searchpath = "/xyz/openbmc_project/";
    int depth = 0;
    std::vector<std::string> ibmCompatible = {compatibleInterface};
    pldm::utils::GetSubTreeResponse response;
    try
    {
        response = pldm::utils::DBusHandler().getSubtree(searchpath, depth,
                                                         ibmCompatible);
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << "getSubtree call failed with, ERROR=" << e.what()
                  << ", PATH=" << searchpath
                  << ", INTERFACE=" << ibmCompatible[0] << "\n";
        return fs::path();
    }

    for (const auto& [objectPath, serviceMap] : response)
    {
        try
        {
            auto value = pldm::utils::DBusHandler()
                             .getDbusProperty<std::vector<std::string>>(
                                 objectPath.c_str(), namesProperty,
                                 ibmCompatible[0].c_str());
            return fs::path{value[0]};
        }
        catch (const sdbusplus::exception_t& e)
        {
            std::cerr << "Error getting Names property , ERROR=" << e.what()
                      << ", PATH=" << objectPath
                      << ", INTERFACE =" << ibmCompatible[0] << "\n";
        }
    }
    return fs::path();
}

/** @brief callback function invoked when interfaces get added from
 *      Entity manager
 *
 *  @param[in] msg - Data associated with subscribed signal
 */
void pldm::responder::oem_ibm_bios::Handler::ibmCompatibleAddedCallback(
    sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path path;

    pldm::utils::InterfacesMap interfaceMap;

    msg.read(path, interfaceMap);

    if (!interfaceMap.contains(compatibleInterface))
    {
        return;
    }
    // Get the "Name" property value of the
    // "xyz.openbmc_project.Configuration.IBMCompatibleSystem" interface
    const auto& properties = interfaceMap.at(compatibleInterface);

    if (!properties.contains(namesProperty))
    {
        return;
    }
    auto names =
        std::get<pldm::utils::Interfaces>(properties.at(namesProperty));

    // get only the first system type
    if (!names.empty())
    {
        systemType = names.front();
    }

    if (!systemType.empty())
    {
        ibmCompatibleMatchConfig.reset();
    }
}

} // namespace oem_ibm_bios
} // namespace responder
} // namespace pldm
