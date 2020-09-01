#include "file_io_type_lid.hpp"

namespace pldm
{
namespace responder
{

template <typename... T>
int executeCmd(T const&... t)
{
    std::stringstream cmd;
    ((cmd << t << " "), ...) << std::endl;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    int rc = pclose(pipe);
    if (WEXITSTATUS(rc))
    {
        std::cerr << "Error executing: ";
        ((std::cerr << " " << t), ...);
        std::cerr << "\n";
        return -1;
    }

    return 0;
}

int LidHandler::assembleImage(const std::string& filePath)
{
    fs::create_directories(imageDirPath);

    // Create the hostfw squashfs image from the LID file
    auto rc = executeCmd("/usr/sbin/mksquashfs", filePath.c_str(),
                         hostfwImagePath.c_str(), "-all-root", "-no-recovery");
    fs::remove(filePath);
    return rc < 0 ? PLDM_ERROR : PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
