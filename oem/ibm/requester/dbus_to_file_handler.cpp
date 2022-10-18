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

static constexpr auto resDumpObjPath =
    "/xyz/openbmc_project/dump/resource/entry";
static constexpr auto resDumpEntry = "com.ibm.Dump.Entry.Resource";
static constexpr auto resDumpProgressIntf =
    "xyz.openbmc_project.Common.Progress";
static constexpr auto resDumpStatus =
    "xyz.openbmc_project.Common.Progress.OperationStatus.Failed";
static constexpr auto certFilePath = "/var/lib/ibm/bmcweb/";
constexpr auto certObjPath = "/xyz/openbmc_project/certs/ca/entry/";
constexpr auto certEntryIntf = "xyz.openbmc_project.Certs.Entry";
static constexpr auto vpdFilePath = "/var/lib/ibm/bmcweb/";
constexpr auto vpdObjPath =
    "/xyz/openbmc_project/inventory/system/chassis/motherboard/vdd_vrm0";
constexpr auto keywrdEntryIntf = "xyz.openbmc_project.Inventory.Manager";
DbusToFileHandler::DbusToFileHandler(
    int mctp_fd, uint8_t mctp_eid, dbus_api::Requester* requester,
    sdbusplus::message::object_path resDumpCurrentObjPath,
    pldm::requester::Handler<pldm::requester::Request>* handler) :
    mctp_fd(mctp_fd),
    mctp_eid(mctp_eid), requester(requester),
    resDumpCurrentObjPath(resDumpCurrentObjPath), handler(handler)
{}

void DbusToFileHandler::sendNewFileAvailableCmd(uint64_t fileSize)
{
    if (requester == NULL)
    {
        std::cerr << "Failed to send resource dump parameters as requester is "
                     "not set";
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.sendNewFileAvailableCmd.SendDumpParametersFail",
            pldm::PelSeverity::ERROR);
        return;
    }
    auto instanceId = requester->getInstanceId(mctp_eid);
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_NEW_FILE_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    uint32_t fileHandle =
        atoi(fs::path((std::string)resDumpCurrentObjPath).filename().c_str());

    auto rc =
        encode_new_file_req(instanceId, PLDM_FILE_TYPE_RESOURCE_DUMP_PARMS,
                            fileHandle, fileSize, request);
    if (rc != PLDM_SUCCESS)
    {
        requester->markFree(mctp_eid, instanceId);
        std::cerr << "Failed to encode_new_file_req, rc = " << rc << std::endl;
        return;
    }

    auto newFileAvailableRespHandler = [this](mctp_eid_t /*eid*/,
                                              const pldm_msg* response,
                                              size_t respMsgLen) {
        if (response == nullptr || !respMsgLen)
        {
            std::cerr
                << "Failed to receive response for NewFileAvailable command \n";
            return;
        }
        uint8_t completionCode{};
        auto rc = decode_new_file_resp(response, respMsgLen, &completionCode);
        if (rc || completionCode)
        {
            std::cerr << "Failed to decode_new_file_resp or"
                      << " Host returned error for new_file_available"
                      << " rc=" << rc
                      << ", cc=" << static_cast<unsigned>(completionCode)
                      << "\n";
            reportResourceDumpFailure("decodeNewFileResp");
        }
    };
    rc = handler->registerRequest(
        mctp_eid, instanceId, PLDM_OEM, PLDM_NEW_FILE_AVAILABLE,
        std::move(requestMsg), std::move(newFileAvailableRespHandler));
    if (rc)
    {
        std::cerr << "Failed to send NewFileAvailable Request to Host \n";
        reportResourceDumpFailure("newFileAvailableRequest");
    }
}

void DbusToFileHandler::reportResourceDumpFailure(std::string str)
{
    std::string s =
        "xyz.openbmc_project.PLDM.Error.ReportResourceDumpFail." + str;

    pldm::utils::reportError(s.c_str(), pldm::PelSeverity::WARNING);

    PropertyValue value{resDumpStatus};
    DBusMapping dbusMapping{resDumpCurrentObjPath, resDumpProgressIntf,
                            "Status", "string"};
    try
    {
        pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to set resource dump operation status, "
                     "ERROR="
                  << e.what() << "\n";
    }
}

void DbusToFileHandler::processNewResourceDump(
    const std::string& vspString, const std::string& resDumpReqPass)
{
    try
    {
        std::string objPath = resDumpCurrentObjPath;
        auto propVal = pldm::utils::DBusHandler().getDbusPropertyVariant(
            objPath.c_str(), "Status", resDumpProgressIntf);
        const auto& curResDumpStatus = std::get<std::string>(propVal);

        if (curResDumpStatus !=
            "xyz.openbmc_project.Common.Progress.OperationStatus.InProgress")
        {
            return;
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        std::cerr << "Error in getting current resource dump status \n";
        return;
    }

    namespace fs = std::filesystem;
    const fs::path resDumpDirPath = "/var/lib/pldm/resourcedump";

    if (!fs::exists(resDumpDirPath))
    {
        fs::create_directories(resDumpDirPath);
    }

    uint32_t fileHandle =
        atoi(fs::path((std::string)resDumpCurrentObjPath).filename().c_str());
    fs::path resDumpFilePath = resDumpDirPath / std::to_string(fileHandle);

    std::ofstream fileHandleFd;
    fileHandleFd.open(resDumpFilePath, std::ios::out | std::ofstream::binary);

    if (!fileHandleFd)
    {
        std::cerr << "resource dump file open error: " << resDumpFilePath
                  << "\n";
        PropertyValue value{resDumpStatus};
        DBusMapping dbusMapping{resDumpCurrentObjPath, resDumpProgressIntf,
                                "Status", "string"};
        try
        {
            pldm::utils::DBusHandler().setDbusProperty(dbusMapping, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "failed to set resource dump operation status, "
                         "ERROR="
                      << e.what() << "\n";
        }
        return;
    }

    // Fill up the file with resource dump parameters and respective sizes
    auto fileFunc = [&fileHandleFd](auto& paramBuf) {
        uint32_t paramSize = paramBuf.size();
        fileHandleFd.write((char*)&paramSize, sizeof(paramSize));
        fileHandleFd << paramBuf;
    };
    fileFunc(vspString);
    fileFunc(resDumpReqPass);

    std::string str;
    if (!resDumpReqPass.empty())
    {
        static constexpr auto acfDirPath = "/etc/acf/service.acf";
        if (fs::exists(acfDirPath))
        {
            std::ifstream file;
            file.open(acfDirPath);
            std::stringstream acfBuf;
            acfBuf << file.rdbuf();
            str = acfBuf.str();
            file.close();
        }
    }
    fileFunc(str);

    fileHandleFd.close();
    size_t fileSize = fs::file_size(resDumpFilePath);

    sendNewFileAvailableCmd(fileSize);
}

void DbusToFileHandler::newCsrFileAvailable(const std::string& csr,
                                            const std::string fileHandle)
{
    namespace fs = std::filesystem;
    std::string dirPath = "/var/lib/ibm/bmcweb";
    const fs::path certDirPath = dirPath;

    if (!fs::exists(certDirPath))
    {
        fs::create_directories(certDirPath);
        fs::permissions(certDirPath,
                        fs::perms::others_read | fs::perms::owner_write);
    }

    fs::path certFilePath = certDirPath / ("CSR_" + fileHandle);
    std::ofstream certFile;

    certFile.open(certFilePath, std::ios::out | std::ofstream::binary);

    if (!certFile)
    {
        std::cerr << "cert file open error: " << certFilePath << "\n";
        return;
    }

    // Add csr to file
    certFile << csr << std::endl;

    certFile.close();
    uint32_t fileSize = fs::file_size(certFilePath);

    newFileAvailableSendToHost(fileSize, (uint32_t)stoi(fileHandle),
                               PLDM_FILE_TYPE_CERT_SIGNING_REQUEST);
}

void DbusToFileHandler::newLicFileAvailable(const std::string& licenseStr)
{
    namespace fs = std::filesystem;
    std::string dirPath = "/var/lib/ibm/cod";
    const fs::path licDirPath = dirPath;

    if (!fs::exists(licDirPath))
    {
        fs::create_directories(licDirPath);
        fs::permissions(licDirPath,
                        fs::perms::others_read | fs::perms::owner_write);
    }

    fs::path licFilePath = licDirPath / "licFile";
    std::ofstream licFile;

    licFile.open(licFilePath, std::ios::out | std::ofstream::binary);

    if (!licFile)
    {
        std::cerr << "License file open error: " << licFilePath << "\n";
        return;
    }

    // Add csr to file
    licFile << licenseStr << std::endl;

    licFile.close();
    uint32_t fileSize = fs::file_size(licFilePath);

    newFileAvailableSendToHost(fileSize, 1, PLDM_FILE_TYPE_COD_LICENSE_KEY);
}

void DbusToFileHandler::newFileAvailableSendToHost(const uint32_t fileSize,
                                                   const uint32_t fileHandle,
                                                   const uint16_t type)
{
    if (requester == NULL)
    {
        std::cerr << "newFileAvailableSendToHost:Failed to send file to host.";
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.SendFileToHostFail",
            pldm::PelSeverity::ERROR);
        return;
    }
    auto instanceId = requester->getInstanceId(mctp_eid);
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_NEW_FILE_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc =
        encode_new_file_req(instanceId, type, fileHandle, fileSize, request);
    if (rc != PLDM_SUCCESS)
    {
        requester->markFree(mctp_eid, instanceId);
        std::cerr
            << "newFileAvailableSendToHost:Failed to encode_new_file_req, rc = "
            << rc << std::endl;
        return;
    }
    std::cout
        << "newFileAvailableSendToHost:Sending Sign CSR request to Host for fileHandle: "
        << fileHandle << std::endl;
    auto newFileAvailableRespHandler = [fileHandle,
                                        type](mctp_eid_t /*eid*/,
                                              const pldm_msg* response,
                                              size_t respMsgLen) {
        if (response == nullptr || !respMsgLen)
        {
            std::cerr << "Failed to receive response for NewFileAvailable "
                         "command\n";
            if (type == PLDM_FILE_TYPE_CERT_SIGNING_REQUEST)
            {
                std::string filePath = certFilePath;
                filePath += "CSR_" + std::to_string(fileHandle);
                fs::remove(filePath);

                DBusMapping dbusMapping{certObjPath +
                                            std::to_string(fileHandle),
                                        certEntryIntf, "Status", "string"};
                PropertyValue value =
                    "xyz.openbmc_project.Certs.Entry.State.Pending";
                try
                {
                    pldm::utils::DBusHandler().setDbusProperty(dbusMapping,
                                                               value);
                }
                catch (const std::exception& e)
                {
                    std::cerr
                        << "newFileAvailableSendToHost:Failed to set status property of certicate entry, "
                           "ERROR="
                        << e.what() << "\n";
                }

                pldm::utils::reportError(
                    "xyz.openbmc_project.PLDM.Error.SendFileToHostFail",
                    pldm::PelSeverity::INFORMATIONAL);
            }
            return;
        }
        uint8_t completionCode{};
        auto rc = decode_new_file_resp(response, respMsgLen, &completionCode);
        if (rc || completionCode)
        {
            std::cerr << "Failed to decode_new_file_resp for file, or"
                      << " Host returned error for new_file_available"
                      << " rc=" << rc
                      << ", cc=" << static_cast<unsigned>(completionCode)
                      << "\n";
            if (rc)
            {
                pldm::utils::reportError(
                    "xyz.openbmc_project.PLDM.Error.DecodeNewFileResponseFail",
                    pldm::PelSeverity::ERROR);
            }
        }
    };
    rc = handler->registerRequest(
        mctp_eid, instanceId, PLDM_OEM, PLDM_NEW_FILE_AVAILABLE,
        std::move(requestMsg), std::move(newFileAvailableRespHandler));
    if (rc)
    {
        std::cerr
            << "newFileAvailableSendToHost:Failed to send NewFileAvailable Request to Host\n";
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.NewFileAvailableRequestFail",
            pldm::PelSeverity::ERROR);
    }
}

void DbusToFileHandler::hbReadFileByType(const uint32_t fileSize,
                                         const uint32_t fileHandle,
                                         const uint16_t type)
{
    if (requester == NULL)
    {
        std::cerr
            << "hbReadFileByType:Request to Hostboot for read file by type failed.";
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.HbReadFileReqFail",
            pldm::PelSeverity::ERROR);
        return;
    }
    auto instanceId = requester->getInstanceId(mctp_eid);
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_RW_FILE_BY_TYPE_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc =
        encode_read_file_req(instanceId, type, fileHandle, fileSize, request);
    if (rc != PLDM_SUCCESS)
    {
        requester->markFree(mctp_eid, instanceId);
        std::cerr << "hbReadFileByType:Failed to encode_read_file_req, rc = "
                  << rc << std::endl;
        return;
    }
    std::cout
        << "hbReadFileByType:Read keyword file by hostboot of fileHandle: "
        << fileHandle << std::endl;
    auto readFileByTypeRespHandler = [fileHandle,
                                      type](mctp_eid_t /*eid*/,
                                            const pldm_msg* response,
                                            size_t respMsgLen) {
        if (response == nullptr || !respMsgLen)
        {
            std::cerr
                << "Failed to receive response for readFileByType command\n";
            return;
        }
        uint8_t completionCode{};
        auto rc = decode_read_file_resp(response, respMsgLen, &completionCode);
        if (rc || completionCode)
        {
            std::cerr
                << "Failed to decode_read_file_resp for file, or Hostboot returned error for readFileByType rc="
                << rc << ", cc=" << static_cast<unsigned>(completionCode)
                << "\n";
            if (rc)
            {
                pldm::utils::reportError(
                    "xyz.openbmc_project.PLDM.Error.DecodeReadFileResponseFail",
                    pldm::PelSeverity::ERROR);
            }
        }
    };
    rc = handler->registerRequest(mctp_eid, instanceId, PLDM_OEM,
                                  PLDM_READ_FILE_BY_TYPE, std::move(requestMsg),
                                  std::move(readFileByTypeRespHandler));
    if (rc)
    {
        std::cerr
            << "readFileByTypeRespHandler:Failed to send ReadFileByType Request to Hostboot\n";
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.ReadFileByTypeRequestFail",
            pldm::PelSeverity::ERROR);
    }
}

} // namespace oem_ibm
} // namespace requester
} // namespace pldm
