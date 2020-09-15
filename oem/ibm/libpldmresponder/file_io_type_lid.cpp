#include "file_io_type_lid.hpp"

#include <fstream>

namespace pldm
{
namespace responder
{

int LidHandler::createSquashFSImage(const std::string& imagePath,
                                    const std::string& filePath)
{
    int status = 0;
    pid_t pid = fork();

    if (pid == 0)
    {
        int fd = open("/dev/null", O_RDWR | O_CLOEXEC);
        if (fd < 0)
        {
            std::cerr << "Failed to open /dev/null, errno=" << -errno << "\n";
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        execl("/usr/sbin/mksquashfs", "mksquashfs", filePath.c_str(),
              imagePath.c_str(), "-all-root", "-no-recovery", nullptr);
        std::cerr << "Failed to execute mksquashfs\n";
        return -1;
    }
    else if (pid > 0)
    {
        waitpid(pid, &status, 0);
        if (WEXITSTATUS(status))
        {
            std::cerr << "Failed to create squashfs image\n";
            return -1;
        }
    }
    else
    {
        std::cerr << "Error occurred during fork\n";
        return -1;
    }

    return 0;
}

int LidHandler::assembleHostFWImage(const std::string& lidPath)
{
    auto imagePath = fs::path(LID_STAGING_DIR) / "image-hostfw";

    auto rc = createSquashFSImage(imagePath, lidPath);
    return rc < 0 ? PLDM_ERROR : PLDM_SUCCESS;
}

int LidHandler::assembleImage(const std::string& lidPath)
{
    std::ifstream lid(lidPath, std::ios::in | std::ios::binary);
    if (!lid)
    {
        return PLDM_ERROR;
    }

    // Read the lid class from the lid header.
    constexpr auto lidClassOffset = 0xE;
    uint8_t lidClass;
    lid.seekg(lidClassOffset);
    lid.read(reinterpret_cast<char*>(&lidClass), sizeof(lidClass));

    // The BMC lids have a lid class value of 0x2-.
    // If the lid is not a BMC one, use it to assemble the host fw image.
    constexpr auto bmcLidClassMask = 0x20;
    if (!(bmcLidClassMask & lidClass))
    {
        return assembleHostFWImage(lidPath);
    }

    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
