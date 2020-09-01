#include "file_io_type_lid.hpp"

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
    return assembleHostFWImage(lidPath);
}

} // namespace responder
} // namespace pldm
