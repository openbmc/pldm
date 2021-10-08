#include "utils.hpp"

#include "libpldm/base.h"

#include "common/utils.hpp"
#include "host-bmc/custom_dbus.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <xyz/openbmc_project/Common/error.hpp>

#include <exception>
#include <fstream>
#include <iostream>

namespace pldm
{
using namespace pldm::dbus;
namespace responder
{
namespace utils
{

static constexpr auto curLicFilePath =
    "/var/lib/pldm/license/current_license.bin";
static constexpr auto newLicFilePath = "/var/lib/pldm/license/new_license.bin";
static constexpr auto newLicJsonFilePath =
    "/var/lib/pldm/license/new_license.json";
static constexpr auto licEntryPath = "/xyz/openbmc_project/license/entry";
static constexpr uint8_t createLic = 1;
static constexpr uint8_t clearLicStatus = 2;

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

using LicJsonObjMap = std::map<fs::path, nlohmann::json>;
LicJsonObjMap licJsonMap;

int setupUnixSocket(const std::string& socketInterface)
{
    int sock;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strnlen(socketInterface.c_str(), sizeof(addr.sun_path)) ==
        sizeof(addr.sun_path))
    {
        std::cerr << "setupUnixSocket: UNIX socket path too long" << std::endl;
        return -1;
    }

    strncpy(addr.sun_path, socketInterface.c_str(), sizeof(addr.sun_path) - 1);

    if ((sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
        std::cerr << "setupUnixSocket: socket() call failed" << std::endl;
        return -1;
    }

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        std::cerr << "setupUnixSocket: bind() call failed" << std::endl;
        close(sock);
        return -1;
    }

    if (listen(sock, 1) == -1)
    {
        std::cerr << "setupUnixSocket: listen() call failed" << std::endl;
        close(sock);
        return -1;
    }

    fd_set rfd;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);
    int nfd = sock + 1;
    int fd = -1;

    int retval = select(nfd, &rfd, NULL, NULL, &tv);
    if (retval < 0)
    {
        std::cerr << "setupUnixSocket: select call failed " << errno
                  << std::endl;
        close(sock);
        return -1;
    }

    if ((retval > 0) && (FD_ISSET(sock, &rfd)))
    {
        fd = accept(sock, NULL, NULL);
        if (fd < 0)
        {
            std::cerr << "setupUnixSocket: accept() call failed " << errno
                      << std::endl;
            close(sock);
            return -1;
        }
        close(sock);
    }
    return fd;
}

int writeToUnixSocket(const int sock, const char* buf, const uint64_t blockSize)
{
    uint64_t i;
    int nwrite = 0;

    for (i = 0; i < blockSize; i = i + nwrite)
    {

        fd_set wfd;
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&wfd);
        FD_SET(sock, &wfd);
        int nfd = sock + 1;

        int retval = select(nfd, NULL, &wfd, NULL, &tv);
        if (retval < 0)
        {
            std::cerr << "writeToUnixSocket: select call failed " << errno
                      << std::endl;
            close(sock);
            return -1;
        }
        if (retval == 0)
        {
            nwrite = 0;
            continue;
        }
        if ((retval > 0) && (FD_ISSET(sock, &wfd)))
        {
            nwrite = write(sock, buf + i, blockSize - i);
            if (nwrite < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                {
                    std::cerr << "writeToUnixSocket: Write call failed with "
                                 "EAGAIN or EWOULDBLOCK or EINTR"
                              << std::endl;
                    nwrite = 0;
                    continue;
                }
                std::cerr << "writeToUnixSocket: Failed to write" << std::endl;
                close(sock);
                return -1;
            }
        }
        else
        {
            nwrite = 0;
        }
    }
    return 0;
}

Json convertBinFileToJson(const fs::path& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    std::streampos fileSize;

    // Get the file size
    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the data into vector from file and convert to json object
    std::vector<uint8_t> vJson(fileSize);
    file.read((char*)&vJson[0], fileSize);
    return Json::from_bson(vJson);
}

void convertJsonToBinaryFile(const Json& jsonData, const fs::path& path)
{
    // Covert the json data to binary format and copy to vector
    std::vector<uint8_t> vJson = {};
    vJson = Json::to_bson(jsonData);

    // Copy the vector to file
    std::ofstream licout(path, std::ios::out | std::ios::binary);
    size_t size = vJson.size();
    licout.write(reinterpret_cast<char*>(&vJson[0]), size * sizeof(vJson[0]));
}

void clearLicenseStatus()
{
    if (!fs::exists(curLicFilePath))
    {
        return;
    }

    auto data = convertBinFileToJson(curLicFilePath);

    const Json empty{};
    const std::vector<Json> emptyList{};

    auto entries = data.value("Licenses", emptyList);
    fs::path path{licEntryPath};

    for (const auto& entry : entries)
    {
        auto licId = entry.value("Id", empty);
        fs::path l_path = path / licId;
        licJsonMap.emplace(l_path, entry);
    }

    createOrUpdateLicenseDbusPaths(clearLicStatus);
}

int createOrUpdateLicenseDbusPaths(const uint8_t& flag)
{
    const Json empty{};
    std::string authTypeAsNoOfDev = "NumberOfDevice";
    struct tm tm;
    time_t licTimeSinceEpoch = 0;

    sdbusplus::com::ibm::License::Entry::server::LicenseEntry::Type licType =
        sdbusplus::com::ibm::License::Entry::server::LicenseEntry::Type::
            Prototype;
    sdbusplus::com::ibm::License::Entry::server::LicenseEntry::AuthorizationType
        licAuthType = sdbusplus::com::ibm::License::Entry::server::
            LicenseEntry::AuthorizationType::Device;
    for (auto const& [key, licJson] : licJsonMap)
    {
        auto licName = licJson.value("Name", empty);

        auto type = licJson.value("Type", empty);
        if (type == "Trial")
        {
            licType = sdbusplus::com::ibm::License::Entry::server::
                LicenseEntry::Type::Trial;
        }
        else if (type == "Commercial")
        {
            licType = sdbusplus::com::ibm::License::Entry::server::
                LicenseEntry::Type::Purchased;
        }
        else
        {
            licType = sdbusplus::com::ibm::License::Entry::server::
                LicenseEntry::Type::Prototype;
        }

        auto authType = licJson.value("AuthType", empty);
        if (authType == "NumberOfDevice")
        {
            licAuthType = sdbusplus::com::ibm::License::Entry::server::
                LicenseEntry::AuthorizationType::Capacity;
        }
        else if (authType == "Unlimited")
        {
            licAuthType = sdbusplus::com::ibm::License::Entry::server::
                LicenseEntry::AuthorizationType::Unlimited;
        }
        else
        {
            licAuthType = sdbusplus::com::ibm::License::Entry::server::
                LicenseEntry::AuthorizationType::Device;
        }

        uint32_t licAuthDevNo = 0;
        if (authType == authTypeAsNoOfDev)
        {
            licAuthDevNo = licJson.value("AuthDeviceNumber", 0);
        }

        auto licSerialNo = licJson.value("SerialNum", "");

        auto expTime = licJson.value("ExpirationTime", "");
        if (!expTime.empty())
        {
            memset(&tm, 0, sizeof(tm));
            strptime(expTime.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm);
            licTimeSinceEpoch = mktime(&tm);
        }

        CustomDBus::getCustomDBus().implementLicInterfaces(
            key, licAuthDevNo, licName, licSerialNo, licTimeSinceEpoch, licType,
            licAuthType);

        auto status = licJson.value("Status", empty);

        // License status is a single entry which needs to be mapped to
        // OperationalStatus and Availability dbus interfaces
        auto licOpStatus = false;
        auto licAvailState = false;
        if ((flag == clearLicStatus) || (status == "Unknown"))
        {
            licOpStatus = false;
            licAvailState = false;
        }
        else if (status == "Enabled")
        {
            licOpStatus = true;
            licAvailState = true;
        }
        else if (status == "Disabled")
        {
            licOpStatus = false;
            licAvailState = true;
        }

        CustomDBus::getCustomDBus().setOperationalStatus(key, licOpStatus);
        CustomDBus::getCustomDBus().setAvailabilityState(key, licAvailState);
    }

    return PLDM_SUCCESS;
}

int createOrUpdateLicenseObjs()
{
    bool l_curFilePresent = true;
    const Json empty{};
    const std::vector<Json> emptyList{};
    std::ifstream jsonFileCurrent;
    Json dataCurrent;
    Json entries;

    if (!fs::exists(curLicFilePath))
    {
        l_curFilePresent = false;
    }

    if (l_curFilePresent == true)
    {
        dataCurrent = convertBinFileToJson(curLicFilePath);
    }

    auto dataNew = convertBinFileToJson(newLicFilePath);

    if (l_curFilePresent == true)
    {
        dataCurrent.merge_patch(dataNew);
        convertJsonToBinaryFile(dataCurrent, curLicFilePath);
        entries = dataCurrent.value("Licenses", emptyList);
    }
    else
    {
        convertJsonToBinaryFile(dataNew, curLicFilePath);
        entries = dataNew.value("Licenses", emptyList);
    }

    fs::path path{licEntryPath};

    for (const auto& entry : entries)
    {
        auto licId = entry.value("Id", empty);
        fs::path l_path = path / licId;
        licJsonMap.emplace(l_path, entry);
    }

    int rc = createOrUpdateLicenseDbusPaths(createLic);
    if (rc == PLDM_SUCCESS)
    {
        fs::copy_file(newLicFilePath, curLicFilePath,
                      fs::copy_options::overwrite_existing);

        if (fs::exists(newLicFilePath))
        {
            fs::remove_all(newLicFilePath);
        }

        if (fs::exists(newLicJsonFilePath))
        {
            fs::remove_all(newLicJsonFilePath);
        }
    }

    return rc;
}

bool checkIfIBMCableCard(const std::string& objPath)
{
    constexpr auto pcieAdapterModelInterface =
        "xyz.openbmc_project.Inventory.Decorator.Asset";
    constexpr auto modelProperty = "Model";

    try
    {
        auto propVal = pldm::utils::DBusHandler().getDbusPropertyVariant(
            objPath.c_str(), modelProperty, pcieAdapterModelInterface);
        const auto& model = std::get<std::string>(propVal);
        if (!model.empty())
        {
            return true;
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        return false;
    }
    return false;
}

void findPortObjects(const std::string& cardObjPath,
                     std::vector<std::string>& portObjects)
{
    static constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
    static constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto portInterface =
        "xyz.openbmc_project.Inventory.Item.Connector";

    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");
        method.append(cardObjPath);
        method.append(0);
        method.append(std::vector<std::string>({portInterface}));
        auto reply = bus.call(method);
        reply.read(portObjects);
    }
    catch (const std::exception& e)
    {
        std::cerr << "no ports under card " << cardObjPath << "\n";
    }
}

bool checkFruPresence(const char* objPath)
{
    // if we enter here with port objects then we need to find the parent
    // and see if the card is present. if so then the port is considered as
    // present. this is so because the ports do not have "Present" property
    std::string nvme("nvme");
    std::string pcieAdapter("pcie_card");
    std::string portStr("cxp_");
    std::string newObjPath = objPath;
    bool isPresent = true;
    if (newObjPath.find(nvme) != std::string::npos)
    {
        return true;
    }
    else if ((newObjPath.find(pcieAdapter) != std::string::npos) &&
             !checkIfIBMCableCard(newObjPath))
    {
        return true; // industry std cards
    }
    else if (newObjPath.find(portStr) != std::string::npos)
    {
        newObjPath = pldm::utils::findParent(objPath);
    }

    // Phyp expects the FRU records for nvme and industry std cards to be always
    // built, irrespective of presence

    static constexpr auto presentInterface =
        "xyz.openbmc_project.Inventory.Item";
    static constexpr auto presentProperty = "Present";
    try
    {
        auto propVal = pldm::utils::DBusHandler().getDbusPropertyVariant(
            newObjPath.c_str(), presentProperty, presentInterface);
        isPresent = std::get<bool>(propVal);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {}
    return isPresent;
}

} // namespace utils
} // namespace responder
} // namespace pldm
