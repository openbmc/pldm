#include "update_manager.hpp"

#include "activation.hpp"
#include "common/utils.hpp"
#include "package_parser.hpp"
#include "systemd_interface.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/source/event.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <spanstream>
#include <string>

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace fw_update
{

namespace fs = std::filesystem;
namespace software = sdbusplus::xyz::openbmc_project::Software::server;
using InvalidArgument =
    sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
using Unavailable = sdbusplus::xyz::openbmc_project::Common::Error::Unavailable;

std::string UpdateManager::getSwId()
{
    return std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

int UpdateManager::processPackage(const std::filesystem::path& packageFilePath)
{
    // If no devices discovered, take no action on the package.
    if (!descriptorMap.size())
    {
        return 0;
    }

    namespace software = sdbusplus::xyz::openbmc_project::Software::server;
    // If a firmware activation of a package is in progress, don't proceed with
    // package processing
    if (activation)
    {
        if (activation->activation() ==
            software::Activation::Activations::Activating)
        {
            error(
                "Activation of PLDM fw update package for version '{VERSION}' already in progress.",
                "VERSION", parser->pkgVersion);
            std::filesystem::remove(packageFilePath);
            return -1;
        }
        else
        {
            resetActivationState();
        }
    }

    package.open(packageFilePath,
                 std::ios::binary | std::ios::in | std::ios::ate);
    if (!package.good())
    {
        error(
            "Failed to open the PLDM fw update package file '{FILE}', error - {ERROR}.",
            "ERROR", errno, "FILE", packageFilePath);
        package.close();
        std::filesystem::remove(packageFilePath);
        return -1;
    }

    uintmax_t packageSize = package.tellg();

    auto swId = getSwId();
    objPath = swRootPath + swId;

    fwPackageFilePath = packageFilePath;

    try
    {
        processStream(package, packageSize);
        return 0;
    }
    catch (sdbusplus::exception_t& e)
    {
        error("Exception occurred while processing the package: {ERROR}",
              "ERROR", e);
        package.close();
        std::filesystem::remove(packageFilePath);
        return -1;
    }
}

std::string UpdateManager::processFd(int fd)
{
    // The D-Bus message owns fd and closes it once the method returns
    auto rawDupFd = dup(fd);
    if (rawDupFd < 0)
    {
        error("Failed to duplicate the package descriptor, error - {ERRNO}",
              "ERRNO", errno);
        throw Unavailable();
    }
    auto dupFd = std::make_unique<pldm::utils::CustomFD>(rawDupFd);

    struct stat sb{};
    if (fstat(rawDupFd, &sb) != 0)
    {
        error("Failed to stat the package file descriptor, error - {ERRNO}",
              "ERRNO", errno);
        throw Unavailable();
    }
    if (!S_ISREG(sb.st_mode) || sb.st_size <= 0)
    {
        error("Package file descriptor is not a non-empty regular file");
        throw InvalidArgument();
    }

    std::unique_ptr<pldm::utils::MMapHandler> map;
    try
    {
        map = std::make_unique<pldm::utils::MMapHandler>(rawDupFd);
    }
    catch (const std::exception& e)
    {
        error("Failed to map the package file descriptor, error - {ERROR}",
              "ERROR", e);
        throw Unavailable();
    }

    packageFd = std::move(dupFd);
    packageMap = std::move(map);
    packageStream = std::make_unique<std::ispanstream>(packageMap->getChars(),
                                                       std::ios::binary);
    try
    {
        return processStreamDefer(*packageStream, packageMap->getSize());
    }
    catch (...)
    {
        releasePackage();
        throw;
    }
}

std::string UpdateManager::processStreamDefer(std::istream& package,
                                              uintmax_t packageSize)
{
    auto swId = getSwId();
    objPath = swRootPath + swId;

    // If no devices discovered, take no action on the package.
    if (!descriptorMap.size())
    {
        error(
            "No devices discovered, cannot process the PLDM fw update package.");
        throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
    }

    updateDeferHandler = std::make_unique<sdeventplus::source::Defer>(
        event, [this, &package, packageSize](sdeventplus::source::EventBase&) {
            this->processStream(package, packageSize);
        });

    return objPath;
}

void UpdateManager::processStream(std::istream& package, uintmax_t packageSize)
{
    startTime = std::chrono::steady_clock::now();
    if (packageSize < sizeof(pldm_package_header_information))
    {
        error(
            "PLDM fw update package length {SIZE} less than the length of the package header information '{PACKAGE_HEADER_INFO_SIZE}'.",
            "SIZE", packageSize, "PACKAGE_HEADER_INFO_SIZE",
            sizeof(pldm_package_header_information));
        activation = std::make_unique<Activation>(
            pldm::utils::DBusHandler::getBus(), objPath,
            software::Activation::Activations::Invalid, this);
        releasePackage();
        throw sdbusplus::error::xyz::openbmc_project::software::update::
            InvalidImage();
    }

    package.seekg(0);
    std::vector<uint8_t> packageHeader(packageSize);
    package.read(reinterpret_cast<char*>(packageHeader.data()), packageSize);

    parser = parsePkgHeader(packageHeader);
    if (parser == nullptr)
    {
        error("Invalid PLDM package header information");
        activation = std::make_unique<Activation>(
            pldm::utils::DBusHandler::getBus(), objPath,
            software::Activation::Activations::Invalid, this);
        releasePackage();
        throw sdbusplus::error::xyz::openbmc_project::software::update::
            InvalidImage();
    }

    package.seekg(0);
    try
    {
        parser->parse(packageHeader);
    }
    catch (const std::exception& e)
    {
        error("Invalid PLDM package header, error - {ERROR}", "ERROR", e);
        activation = std::make_unique<Activation>(
            pldm::utils::DBusHandler::getBus(), objPath,
            software::Activation::Activations::Invalid, this);
        releasePackage();
        throw sdbusplus::error::xyz::openbmc_project::software::update::
            InvalidImage();
    }

    auto deviceUpdaterInfos =
        associatePkgToDevices(parser->getFwDeviceIDRecords(), descriptorMap,
                              totalNumComponentUpdates);
    if (!deviceUpdaterInfos.size())
    {
        error(
            "No matching devices found with the PLDM firmware update package");
        activation = std::make_unique<Activation>(
            pldm::utils::DBusHandler::getBus(), objPath,
            software::Activation::Activations::Invalid, this);
        releasePackage();
        throw sdbusplus::error::xyz::openbmc_project::software::update::
            Incompatible();
    }

    const auto& fwDeviceIDRecords = parser->getFwDeviceIDRecords();
    const auto& compImageInfos = parser->getComponentImageInfos();

    static constexpr uint32_t MAXIMUM_TRANSFER_SIZE = 4096;
    for (const auto& deviceUpdaterInfo : deviceUpdaterInfos)
    {
        const auto& fwDeviceIDRecord =
            fwDeviceIDRecords[deviceUpdaterInfo.second];
        auto search = componentInfoMap.find(deviceUpdaterInfo.first);
        deviceUpdaterMap.emplace(
            deviceUpdaterInfo.first,
            std::make_unique<DeviceUpdater>(
                deviceUpdaterInfo.first, package, fwDeviceIDRecord,
                compImageInfos, search->second, MAXIMUM_TRANSFER_SIZE, this));
    }

    updateInProgress = true;
    activation = std::make_unique<Activation>(
        pldm::utils::DBusHandler::getBus(), objPath,
        software::Activation::Activations::Ready, this);
    activationProgress = std::make_unique<ActivationProgress>(
        pldm::utils::DBusHandler::getBus(), objPath);

#ifndef FW_UPDATE_INOTIFY_ENABLED
    activation->activation(software::Activation::Activations::Activating);
#endif
}

DeviceUpdaterInfos UpdateManager::associatePkgToDevices(
    const FirmwareDeviceIDRecords& fwDeviceIDRecords,
    const DescriptorMap& descriptorMap,
    TotalComponentUpdates& totalNumComponentUpdates)
{
    DeviceUpdaterInfos deviceUpdaterInfos;
    for (size_t index = 0; index < fwDeviceIDRecords.size(); ++index)
    {
        const auto& deviceIDDescriptors =
            std::get<Descriptors>(fwDeviceIDRecords[index]);
        for (const auto& [eid, descriptors] : descriptorMap)
        {
            if (std::includes(descriptors.begin(), descriptors.end(),
                              deviceIDDescriptors.begin(),
                              deviceIDDescriptors.end()))
            {
                deviceUpdaterInfos.emplace_back(std::make_pair(eid, index));
                const auto& applicableComponents =
                    std::get<ApplicableComponents>(fwDeviceIDRecords[index]);
                totalNumComponentUpdates += applicableComponents.size();
            }
        }
    }
    return deviceUpdaterInfos;
}

void UpdateManager::updateDeviceCompletion(mctp_eid_t eid, bool status)
{
    deviceUpdateCompletionMap.emplace(eid, status);
    if (deviceUpdateCompletionMap.size() == deviceUpdaterMap.size())
    {
        for (const auto& [eid, status] : deviceUpdateCompletionMap)
        {
            if (!status)
            {
                info("Firmware update failed on eid {EID}", "EID", eid);
                completeUpdate(false);
                return;
            }
        }

        if (!postConditionPath.empty())
        {
            SystemdInterface::getInstance(pldm::utils::DBusHandler::getBus())
                .execute(
                    postConditionPath, conditionArg,
                    [this](bool conditionSuccess) {
                        if (!updateInProgress)
                        {
                            return;
                        }

                        if (!conditionSuccess)
                        {
                            error("Post-update condition failed for {PATH}",
                                  "PATH", postConditionPath);
                        }

                        completeUpdate(conditionSuccess);
                    });
            return;
        }

        completeUpdate(true);
    }
    return;
}

Response UpdateManager::handleRequest(mctp_eid_t eid, uint8_t command,
                                      const pldm_msg* request, size_t reqMsgLen)
{
    Response response(sizeof(pldm_msg), 0);
    if (deviceUpdaterMap.contains(eid))
    {
        auto search = deviceUpdaterMap.find(eid);
        if (command == PLDM_REQUEST_FIRMWARE_DATA)
        {
            return search->second->requestFwData(request, reqMsgLen);
        }
        else if (command == PLDM_TRANSFER_COMPLETE)
        {
            return search->second->transferComplete(request, reqMsgLen);
        }
        else if (command == PLDM_VERIFY_COMPLETE)
        {
            return search->second->verifyComplete(request, reqMsgLen);
        }
        else if (command == PLDM_APPLY_COMPLETE)
        {
            return search->second->applyComplete(request, reqMsgLen);
        }
        else
        {
            auto ptr = new (response.data()) pldm_msg;
            auto rc = encode_cc_only_resp(
                request->hdr.instance_id, request->hdr.type,
                request->hdr.command, PLDM_ERROR_INVALID_DATA, ptr);
            assert(rc == PLDM_SUCCESS);
        }
    }
    else
    {
        auto ptr = new (response.data()) pldm_msg;
        auto rc = encode_cc_only_resp(request->hdr.instance_id,
                                      request->hdr.type, +request->hdr.command,
                                      PLDM_FWUP_COMMAND_NOT_EXPECTED, ptr);
        assert(rc == PLDM_SUCCESS);
    }

    return response;
}

void UpdateManager::activatePackage()
{
    if (!updateInProgress)
    {
        return;
    }

    if (!preConditionPath.empty())
    {
        SystemdInterface::getInstance(pldm::utils::DBusHandler::getBus())
            .execute(preConditionPath, conditionArg, [this](bool success) {
                if (!updateInProgress)
                {
                    return;
                }

                if (!success)
                {
                    error("Pre-update condition failed for {PATH}", "PATH",
                          preConditionPath);
                    completeUpdate(false);
                    return;
                }

                startFirmwareUpdate();
            });
        return;
    }

    startFirmwareUpdate();
}

void UpdateManager::startFirmwareUpdate()
{
    if (!updateInProgress)
    {
        return;
    }

    startTime = std::chrono::steady_clock::now();
    for (const auto& [eid, deviceUpdaterPtr] : deviceUpdaterMap)
    {
        deviceUpdaterPtr->startFwUpdateFlow();
    }
}

void UpdateManager::completeUpdate(bool status)
{
    if (!updateInProgress)
    {
        return;
    }

    updateInProgress = false;
    auto endTime = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration<double, std::milli>(endTime - startTime).count();
    info("Firmware update time: {DURATION}ms", "DURATION", dur);
    activation->activation(status ? software::Activation::Activations::Active
                                  : software::Activation::Activations::Failed);

    if (taskCompletionCallback)
    {
        taskCompletionCallback();
    }

    // The completing DeviceUpdater may still be executing; release its
    // package from the event loop instead
    releaseDeferHandler = std::make_unique<sdeventplus::source::Defer>(
        event, [this](sdeventplus::source::EventBase&) {
            releaseDeferHandler.reset();
            releasePackage();
        });
}

void UpdateManager::resetActivationState()
{
    // Deferred work of the previous update must not touch the next package
    releaseDeferHandler.reset();
    updateDeferHandler.reset();
    updateInProgress = false;
    lastProgress = 0;
    activation.reset();
    activationProgress.reset();
    objPath.clear();
    releasePackage();
    totalNumComponentUpdates = 0;
}

void UpdateManager::releasePackage()
{
    deviceUpdaterMap.clear();
    deviceUpdateCompletionMap.clear();
    parser.reset();
    packageStream.reset();
    packageMap.reset();
    packageFd.reset();
    if (package.is_open())
    {
        package.close();
    }
    std::filesystem::remove(fwPackageFilePath);
}

void UpdateManager::updateActivationProgress()
{
    using mapEl = std::pair<const mctp_eid_t, std::unique_ptr<DeviceUpdater>>;
    auto min = std::ranges::min_element(
        deviceUpdaterMap, [](const mapEl& lhs, const mapEl& rhs) {
            return lhs.second->getProgress() < rhs.second->getProgress();
        });

    if (min == deviceUpdaterMap.end())
    {
        return;
    }

    uint8_t progress = min->second->getProgress();
    if (progress != lastProgress)
    {
        activationProgress->progress(progress);
        lastProgress = progress;
    }
}

} // namespace fw_update

} // namespace pldm
