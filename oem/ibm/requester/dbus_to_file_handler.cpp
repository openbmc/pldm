#include "dbus_to_file_handler.hpp"

#include "libpldm/requester/pldm.h"
#include "oem/ibm/libpldm/file_io.h"

#include "common/utils.hpp"

namespace pldm
{
namespace requester
{
namespace oem_ibm
{

using namespace pldm::utils;
using namespace sdbusplus::bus::match::rules;

static constexpr auto dumpEntry = "xyz.openbmc_project.Dump.Entry";
static constexpr auto resDumpObjPath = "/xyz/openbmc_project/dump/resource";
static constexpr auto resDumpEntry = "xyz.openbmc_project.Dump.Entry.Resource";

static std::string findResDumpObjPath()
{
    static constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
    static constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        std::vector<std::string> paths;
        auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");
        method.append(resDumpObjPath);
        method.append(0);
        method.append(std::vector<std::string>({resDumpEntry}));
        auto reply = bus.call(method);
        reply.read(paths);
        for (const auto& path : paths)
        {
            return path;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to DUMP manager, ERROR="
                  << e.what() << "\n";
    }

    std::cerr << "failed to find dump object for resource dump id \n";
    return {};
}

DbusToFileHandler::DbusToFileHandler(int mctp_fd, uint8_t mctp_eid,
                                     Requester& requester) :
    mctp_fd(mctp_fd),
    mctp_eid(mctp_eid), requester(requester)
{}

void DbusToFileHandler::sendNewFileAvailableCmd(uint64_t fileSize)
{
    auto instanceId = requester.getInstanceId(mctp_eid);
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_NEW_FILE_REQ_BYTES + fileSize);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    uint32_t fileHandle = 1;

    auto rc =
        encode_new_file_req(instanceId, PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS,
                            fileHandle, fileSize, request);
    if (rc != PLDM_SUCCESS)
    {
        requester.markFree(mctp_eid, instanceId);
        std::cerr << "Failed to encode_new_file_req, rc = " << rc << std::endl;
        return;
    }

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};

    auto requesterRc =
        pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(), requestMsg.size(),
                       &responseMsg, &responseMsgSize);
    std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{responseMsg,
                                                                  std::free};

    requester.markFree(mctp_eid, instanceId);
    if (requesterRc != PLDM_REQUESTER_SUCCESS)
    {
        std::cerr << "Failed to send msg to resource dump parameters, rc = "
                  << requesterRc << std::endl;
        return;
    }
    size_t length{};
    uint8_t completionCode{};
    auto responsePtr = reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
    rc = decode_new_file_resp(responsePtr, length, &completionCode);

    if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
    {
        std::cerr << "Failed to decode_new_file_resp: "
                  << "rc=" << rc
                  << ", cc=" << static_cast<unsigned>(completionCode)
                  << std::endl;
    }
}

int DbusToFileHandler::processNewResourceDump()
{
    auto path = findResDumpObjPath();
    if (path.empty())
    {
        return {};
    }

    std::string vspString{};
    try
    {
        vspString = pldm::utils::DBusHandler().getDbusProperty<std::string>(
            path.c_str(), "VSPString", dumpEntry);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to get VSPString, ERROR="
                  << e.what() << "\n";
    }

    std::string resDumpReqPass{};
    try
    {
        resDumpReqPass =
            pldm::utils::DBusHandler().getDbusProperty<std::string>(
                path.c_str(), "Password", dumpEntry);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to get resource dump request "
                     "pass, ERROR="
                  << e.what() << "\n";
    }

    namespace fs = std::filesystem;
    fs::path resDumpDirPath = "/var/lib/pldm/resourcedump";

    if (!fs::exists(resDumpDirPath))
    {
        fs::create_directories(resDumpDirPath);
    }

    fs::path resDumpFilePath = resDumpDirPath / "1";

    // TODO : Need to do the error handling
    std::ofstream fileHandle;
    fileHandle.open(resDumpFilePath);

    fileHandle << sizeof(vspString);
    fileHandle << vspString;
    fileHandle << sizeof(resDumpReqPass);
    fileHandle << resDumpReqPass;

    size_t fileSize = fs::file_size(resDumpFilePath);
    fileHandle.close();

    sendNewFileAvailableCmd(fileSize);
    return 0;
}

} // namespace oem_ibm
} // namespace requester
} // namespace pldm
