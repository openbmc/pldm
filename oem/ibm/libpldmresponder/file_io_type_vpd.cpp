#include "file_io_type_vpd.hpp"

#include "libpldm/base.h"
#include "libpldm/file_io.h"

#include "common/utils.hpp"

#include <stdint.h>

#include <phosphor-logging/lg2.hpp>

#include <iostream>

PHOSPHOR_LOG2_USING;

typedef uint8_t byte;

namespace pldm
{
namespace responder
{
int keywordHandler::read(uint32_t offset, uint32_t& length, Response& response,
                         oem_platform::Handler* /*oemPlatformHandler*/)
{
    const char* keywrdObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard";
    const char* keywrdPropName = "PD_D";
    const char* keywrdInterface = "com.ibm.ipzvpd.PSPD";

    std::variant<std::vector<byte>> keywrd;

    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        auto service = pldm::utils::DBusHandler().getService(keywrdObjPath,
                                                             keywrdInterface);
        auto method = bus.new_method_call(service.c_str(), keywrdObjPath,
                                          "org.freedesktop.DBus.Properties",
                                          "Get");
        method.append(keywrdInterface, keywrdPropName);
        auto reply = bus.call(method);
        reply.read(keywrd);
    }
    catch (const std::exception& e)
    {
        error(
            "Get keyword error from dbus interface :{KEYWORD_INTF} ERROR={ERR_EXCEP}",
            "KEYWORD_INTF", keywrdInterface, "ERR_EXCEP", e.what());
    }

    uint32_t keywrdSize = std::get<std::vector<byte>>(keywrd).size();
    if (length < keywrdSize)
    {
        error(
            "length requested is less the keyword size, length:{LEN} keyword size:{KEYWORD_SIZE}",
            "LEN", length, "KEYWORD_SIZE", keywrdSize);
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

    std::ofstream keywrdFile(keywrdFilePath);
    auto fd = open(keywrdFilePath, std::ios::out | std::ofstream::binary);
    if (!keywrdFile)
    {
        error("VPD keyword file open error: {KEYWORD_FILE_PATH} errno: {ERR}",
              "KEYWORD_FILE_PATH", keywrdFilePath, "ERR", errno);
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.readKeywordHandler.keywordFileOpenError");
        return PLDM_ERROR;
    }

    if (offset > keywrdSize)
    {
        error("Offset exceeds file size, OFFSET={OFFSET} FILE_SIZE={FILE_SIZE}",
              "OFFSET", offset, "FILE_SIZE", keywrdSize);
        return PLDM_DATA_OUT_OF_RANGE;
    }

    // length of keyword data should be same as keyword data size in dbus
    // object
    length = static_cast<uint32_t>(keywrdSize) - offset;

    auto returnCode = lseek(fd, offset, SEEK_SET);
    if (returnCode == -1)
    {
        error("Could not find keyword data at given offset. File Seek failed");
        return PLDM_ERROR;
    }

    keywrdFile.write((const char*)std::get<std::vector<byte>>(keywrd).data(),
                     keywrdSize);
    if (keywrdFile.bad())
    {
        error("Error while writing to file: {KEYWORD_FILE_PATH}",
              "KEYWORD_FILE_PATH", keywrdFilePath);
    }
    keywrdFile.close();

    auto rc = readFile(keywrdFilePath, offset, keywrdSize, response);
    fs::remove(keywrdFilePath);
    if (rc)
    {
        error("Read error for keyword file with size: {KEYWORD_SIZE}",
              "KEYWORD_SIZE", keywrdSize);
        pldm::utils::reportError(
            "xyz.openbmc_project.PLDM.Error.readKeywordHandler.keywordFileReadError");
        return PLDM_ERROR;
    }
    return PLDM_SUCCESS;
}
} // namespace responder
} // namespace pldm
