#include "code_updater.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

sdbusplus::message::object_path
    CodeUpdater::startUpdate(sdbusplus::message::unix_fd image,
                             ApplyTimeIntf::RequestedApplyTimes /*applyTime*/)
{
    info("Starting update for image {FD}", "FD", static_cast<int>(image));

    auto objPath = "/xyz/openbmc_project/software/updates/test";

    writeToFile(image, "/tmp/pldm_package");
    close(image);
    updateManager->processPackage("/tmp/pldm_package");

    return sdbusplus::message::object_path(objPath);
}

bool CodeUpdater::writeToFile(int imageFd, const std::string& path)
{
    FILE* outStream = popen(path.c_str(), "w");
    if (outStream == nullptr)
    {
        error("Failed to open file {PATH}, ERRNO={ERRNO}", "PATH", path,
              "ERRNO", errno);
        return false;
    }

    const int BUFFER_SIZE = 100;
    ssize_t bytesRead;
    char buffer[BUFFER_SIZE];

    // Read from source and write to destination
    while ((bytesRead = read(imageFd, buffer, BUFFER_SIZE)) > 0)
    {
        if (fwrite(buffer, 1, bytesRead, outStream) != (size_t)bytesRead)
        {
            error("Failed to write to file {PATH}, ERRNO={ERRNO}", "PATH", path,
                  "ERRNO", errno);
            return false;
        }
    }

    if (bytesRead < 0)
    {
        std::cerr << "Failed to read from source file: " << strerror(errno)
                  << std::endl;
        return false;
    }

    return true;
}

} // namespace pldm::fw_update
                              