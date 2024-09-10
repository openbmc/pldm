#include "code_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sstream>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

sdbusplus::message::object_path
    CodeUpdater::startUpdate(sdbusplus::message::unix_fd image,
                             ApplyTimeIntf::RequestedApplyTimes /*applyTime*/)
{
    info("Starting update for image {FD}", "FD", static_cast<int>(image));
    
    std::stringstream sstream(std::ios::in|std::ios::out|std::ios::binary);
    writeToSstream(image, sstream);

    updateManager->processStream(std::move(sstream));

    return sdbusplus::message::object_path(objPath);
}

bool CodeUpdater::writeToSstream(int imageFd, std::stringstream& sstream)
{
    if(!sstream)
    {
        error("Invalid stringstream");
        return false;
    }

    const int BUFFER_SIZE = 0x200;
    ssize_t bytesRead;
    char buffer[BUFFER_SIZE];
    // Read from source and write to destination
    while ((bytesRead = read(imageFd, buffer, BUFFER_SIZE)) > 0)
    {
        sstream.write(buffer,bytesRead);
    }
    sstream.seekg(0);
    sstream.seekp(0);
    if (bytesRead < 0)
    {
        std::cerr << "Failed to read from source file: " << strerror(errno)
                  << std::endl;
        return false;
    }

    return true;
}

} // namespace pldm::fw_update
                              