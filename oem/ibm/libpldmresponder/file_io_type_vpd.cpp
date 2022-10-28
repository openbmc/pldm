#include "file_io_type_vpd.hpp"

#include "libpldm/base.h"
#include "libpldm/file_io.h"

#include "common/utils.hpp"

#include <stdint.h>

#include <iostream>

typedef uint8_t byte;

namespace pldm
{
namespace responder
{
int keywordHandler::read(uint32_t offset, uint32_t& length, Response& response,
                         oem_platform::Handler* /*oemPlatformHandler*/)
{
    const char* keywrdObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/vdd_vrm0";
    const char* keywrdPropName = "PD_D";
    const char* keywrdInterface = "com.ibm.ipzvpd.VPRI";

    std::variant<std::vector<byte>> keywrd;

    try
    {
        auto& bus = DBusHandler::getBus();
        auto service = pldm::utils::DBusHandler().getService(keywrdObjPath,
                                                             keywrdInterface);
        auto method =
            bus.new_method_call(service.c_str(), keywrdObjPath,
                                "org.freedesktop.DBus.Properties", "Get");
        method.append(keywrdInterface, keywrdPropName);
        auto reply = bus.call(method);
        reply.read(keywrd);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get keyword error from dbus interface : "
                  << keywrdInterface << " ERROR= " << e.what() << std::endl;
    }

    auto keywrdSize = std::get<std::vector<byte>>(keywrd).size();

    if (length < keywrdSize)
    {
        return PLDM_ERROR_INVALID_DATA;
    }

    namespace fs = std::filesystem;
    constexpr auto keywrdDirPath = "/tmp/pldm/";
    constexpr auto keywrdFilePath = "/tmp/pldm/vpdKeywrd.bin";

    if (!fs::exists(keywrdDirPath))
    {
        fs::create_directories(keywrdDirPath);
        fs::permissions(keywrdDirPath,
                        fs::perms::others_read | fs::perms::owner_write);
    }

    std::ofstream keywrdFile("vpdKeywrd.bin");
    keywrdFile.open(keywrdFilePath, std::ios::out | std::ofstream::binary);
    if (!keywrdFile)
    {
        std::cerr << "VPD keyword file open error: " << keywrdFilePath
                  << " errno: " << errno << std::endl;
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.readKeywordHandler.keywordFileOpenError",
            pldm::PelSeverity::ERROR);
        return PLDM_ERROR;
    }
    keywrdFile.write((const char*)std::get<std::vector<byte>>(keywrd).data(),
                     keywrdSize);
    if (keywrdFile.bad())
    {
        std::cerr << "Error while writing to file: " << keywrdFilePath
                  << std::endl;
    }
    keywrdFile.close();

    auto rc = readFile(keywrdFilePath, offset, keywrdSize, response);
    fs::remove(keywrdFilePath);
    if (rc)
    {
        std::cerr << "Read error for keyword file with size: " << keywrdSize
                  << std::endl;
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.readKeywordHandler.keywordFileReadError",
            pldm::PelSeverity::ERROR);
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}
} // namespace responder
} // namespace pldm
