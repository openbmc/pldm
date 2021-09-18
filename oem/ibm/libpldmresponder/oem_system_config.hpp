#include <config.h>

#include <common/utils.hpp>

#include <filesystem>

namespace pldm
{

namespace oem_system
{
namespace fs = std::filesystem;

static constexpr auto compatibleInterface =
    "xyz.openbmc_project.Configuration.IBMCompatibleSystem";
static constexpr auto namesProperty = "Names";
static constexpr auto orgFreeDesktopInterface =
    "org.freedesktop.DBus.Properties";
static constexpr auto getMethod = "Get";

class IBMCompatible
{
  public:
    IBMCompatible()
    {
        setConfigDirectory();
    }

    const fs::path& getConfigDir()
    {
        return confDir;
    }

  protected:
    void setConfigDirectory()
    {
        static constexpr auto searchpath = "/xyz/openbmc_project/";
        int depth = 0;
        std::vector<std::string> ibmCompatible = {compatibleInterface};
        pldm::utils::GetSubTreeResponse response =
            pldm::utils::DBusHandler().getSubtree(searchpath, depth,
                                                  ibmCompatible);
        auto& bus = pldm::utils::DBusHandler::getBus();
        std::variant<std::vector<std::string>> value;

        for (const auto& [objectPath, serviceMap] : response)
        {
            try
            {
                auto method = bus.new_method_call(
                    serviceMap[0].first.c_str(), objectPath.c_str(),
                    orgFreeDesktopInterface, getMethod);
                method.append(ibmCompatible[0].c_str(), namesProperty);
                auto reply = bus.call(method);
                reply.read(value);
                break;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error getting Names property , PATH="
                          << objectPath
                          << " Compatible interface =" << ibmCompatible[0]
                          << "\n";
                return;
            }
        }
        confDir = fs::path{PDR_JSONS_DIR} /
                  fs::path{std::get<std::vector<std::string>>(value)[0]};
    }

    /**
     * @brief The JSON config directory
     */
    fs::path confDir;
};
} // namespace oem_system

} // namespace pldm
