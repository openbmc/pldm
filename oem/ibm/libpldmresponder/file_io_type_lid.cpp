#include "file_io_type_lid.hpp"

#include <arpa/inet.h>

#include <fstream>

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
    std::ifstream ifs(filePath, std::ios::in | std::ios::binary);
    if (!ifs)
    {
        return PLDM_ERROR;
    }

    lidHeader header;
    ifs.seekg(0);
    ifs.read(reinterpret_cast<char*>(&header), sizeof(header));

    constexpr auto magicNumber = 0x0222;
    if (htons(header.magicNumber) != magicNumber)
    {
        return PLDM_ERROR;
    }

    // File size should be the value of lid size minus the header size
    auto fileSize = fs::file_size(filePath);
    fileSize -= htonl(header.headerSize);
    if (fileSize < htonl(header.lidSize))
    {
        // File is not completely written yet
        return PLDM_SUCCESS;
    }

    fs::create_directories(imageDirPath);

    // Create the hostfw squashfs image from the LID file
    auto rc = executeCmd("/usr/sbin/mksquashfs", filePath.c_str(),
                         hostfwImagePath.c_str(), "-all-root", "-no-recovery");
    fs::remove(filePath);
    return rc < 0 ? PLDM_ERROR : PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
