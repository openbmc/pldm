#include "file_io_type_progress_src.hpp"

#include "common/utils.hpp"

#include <iostream>

namespace pldm
{

namespace responder
{

int ProgressCodeHandler::setRawBootProperty(
    const std::vector<uint8_t>& progressCodeBuffer)
{
    static constexpr auto RawObjectPath = "/xyz/openbmc_project/state/boot/raw";
    static constexpr auto RawInterface = "xyz.openbmc_project.State.Boot.Raw";
    static constexpr auto RawProperty = "Value";

    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        auto service =
            pldm::utils::DBusHandler().getService(RawObjectPath, RawInterface);
        auto method = bus.new_method_call(service.c_str(), RawObjectPath,
                                          RawInterface, RawProperty);
        method.append(progressCodeBuffer);

        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to host-postd daemon, ERROR="
                  << e.what() << "\n";
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

int ProgressCodeHandler::write(const char* buffer, uint32_t /*offset*/,
                               uint32_t& length,
                               oem_platform::Handler* /*oemPlatformHandler*/)
{
    // read the data from the pointed location
    std::vector<uint8_t> progressCodeBuffer(buffer, buffer + length);

    return setRawBootProperty(progressCodeBuffer);
}

} // namespace responder
} // namespace pldm