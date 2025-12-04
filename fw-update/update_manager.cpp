#include "update_manager.hpp"

#include "activation.hpp"
#include "common/utils.hpp"
#include "package_parser.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/source/event.hpp>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace fw_update
{

namespace fs = std::filesystem;
namespace software = sdbusplus::xyz::openbmc_project::Software::server;

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
            clearActivationInfo();
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
        parser.reset();
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
        parser.reset();
        throw sdbusplus::error::xyz::openbmc_project::software::update::
            InvalidImage();
    }

    package.seekg(0);
    try
    {
        parser->parse(packageHeader, packageSize);
    }
    catch (const std::exception& e)
    {
        error("Invalid PLDM package header, error - {ERROR}", "ERROR", e);
        activation = std::make_unique<Activation>(
            pldm::utils::DBusHandler::getBus(), objPath,
            software::Activation::Activations::Invalid, this);
        parser.reset();
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
        parser.reset();
        throw sdbusplus::error::xyz::openbmc_project::software::update::
            Incompatible();
    }

    const auto& fwDeviceIDRecords = parser->getFwDeviceIDRecords();
    const auto& compImageInfos = parser->getComponentImageInfos();

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
                activation->activation(
                    software::Activation::Activations::Failed);
                return;
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto dur =
            std::chrono::duration<double, std::milli>(endTime - startTime)
                .count();
        info("Firmware update time: {DURATION}ms", "DURATION", dur);
        activation->activation(software::Activation::Activations::Active);
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
    startTime = std::chrono::steady_clock::now();
    for (const auto& [eid, deviceUpdaterPtr] : deviceUpdaterMap)
    {
        deviceUpdaterPtr->startFwUpdateFlow();
    }
}

void UpdateManager::clearActivationInfo()
{
    activation.reset();
    activationProgress.reset();
    objPath.clear();

    if (package.is_open())
    {
        package.close();
    }
    deviceUpdaterMap.clear();
    deviceUpdateCompletionMap.clear();
    parser.reset();
    std::filesystem::remove(fwPackageFilePath);
    totalNumComponentUpdates = 0;
    compUpdateCompletedCount = 0;
}

void UpdateManager::updateActivationProgress()
{
    auto min = std::ranges::min_element(
        deviceUpdaterMap,
        [](const std::pair<const mctp_eid_t, std::unique_ptr<DeviceUpdater>>&
               lhs,
           const std::pair<const mctp_eid_t, std::unique_ptr<DeviceUpdater>>&
               rhs) {
            return lhs.second->getProgress() < rhs.second->getProgress();
        });

    if (min == deviceUpdaterMap.end())
    {
        return;
    }

    activationProgress->progress(min->second->getProgress());
}

} // namespace fw_update

} // namespace pldm
