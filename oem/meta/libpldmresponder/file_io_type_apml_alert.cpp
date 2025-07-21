#include "file_io_type_apml_alert.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

constexpr auto CPUID_SIZE = 16;

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

int APMLAlertHandler::write(const message& data)
{
    FILE* fp;
    char buffer[1024];
    auto fru = tid / 10;

    if (data.size() != CPUID_SIZE)
    {
        error("Invalid length for CPUID");
    }

    // Convert to hex string
    std::string CPUIDString;
    for (size_t i = 0; i < data.size(); ++i)
    {
        CPUIDString += std::format("0x{:02x}", data[i]);
        if (i != data.size() - 1)
        {
            CPUIDString += " ";
        }
    }

    auto command = "amd-ras --fru " + std::to_string(fru) + " --ncpu 1 --cid " +
                   CPUIDString;

    auto lock_file = "/tmp/amd-ras" + std::to_string(fru) + ".lock";
    int fd = open(lock_file.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("open lock for amd-ras failed");
        return PLDM_ERROR;
    }

    // obtain lock
    if (flock(fd, LOCK_EX) == -1)
    {
        perror("flock for amd-ras failed");
        close(fd);
        return PLDM_ERROR;
    }

    // run amd-ras
    fp = popen(command.c_str(), "r");
    if (fp == NULL)
    {
        perror("popen() failed for amd-ras");
        flock(fd, LOCK_UN);
        close(fd);
        return PLDM_ERROR;
    }

    std::string output;
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        output += buffer;
    }
    pclose(fp);

    // release lock
    if (flock(fd, LOCK_UN) == -1)
    {
        perror("flock for amd-ras failed");
        close(fd);
        return PLDM_ERROR;
    }

    close(fd);

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
