#include "file_io_type_progress_src.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace responder
{

int ProgressCodeHandler::setRawBootProperty(
    const std::tuple<std::vector<uint8_t>, std::vector<uint8_t>>&
        progressCodeBuffer)
{
    static constexpr auto RawObjectPath =
        "/xyz/openbmc_project/state/boot/raw0";
    static constexpr auto RawInterface = "xyz.openbmc_project.State.Boot.Raw";
    static constexpr auto FreedesktopInterface =
        "org.freedesktop.DBus.Properties";
    static constexpr auto RawProperty = "Value";
    static constexpr auto SetMethod = "Set";

    auto& bus = pldm::utils::DBusHandler::getBus();

    try
    {
        auto service =
            pldm::utils::DBusHandler().getService(RawObjectPath, RawInterface);
        auto method = bus.new_method_call(service.c_str(), RawObjectPath,
                                          FreedesktopInterface, SetMethod);
        method.append(
            RawInterface, RawProperty,
            std::variant<
                std::tuple<std::vector<uint8_t>, std::vector<uint8_t>>>(
                progressCodeBuffer));

        bus.call_noreply(method, dbusTimeout);
    }
    catch (const std::exception& e)
    {
        error(
            "Failed to make a d-bus call to host-postd daemon, error - {ERROR}",
            "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

int ProgressCodeHandler::write(const char* buffer, uint32_t /*offset*/,
                               uint32_t& length,
                               oem_platform::Handler* /*oemPlatformHandler*/)
{
    static constexpr auto StartOffset = 40;
    static constexpr auto EndOffset = 48;
    if (buffer != nullptr)
    {
        // read the data from the pointed location
        std::vector<uint8_t> secondaryCode(buffer, buffer + length);

        // Get the primary code from the offset 40 bytes in the received buffer

        std::vector<uint8_t> primaryCode(secondaryCode.begin() + StartOffset,
                                         secondaryCode.begin() + EndOffset);

        return setRawBootProperty(std::make_tuple(primaryCode, secondaryCode));
    }
    return PLDM_ERROR;
}

} // namespace responder
} // namespace pldm
