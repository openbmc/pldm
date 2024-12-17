#include "update_manager.hpp"

#include "activation.hpp"
#include "common/utils.hpp"
#include "package_parser.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace fw_update
{

namespace fs = std::filesystem;
namespace software = sdbusplus::xyz::openbmc_project::Software::server;

int UpdateManager::processPackage(const std::filesystem::path& packageFilePath)
{
    package.open(packageFilePath, std::ios::binary);
    if (!package)
    {
        error(
            "Failed to open the PLDM fw update package file '{FILE}', error - {ERROR}.",
            "ERROR", errno, "FILE", packageFilePath);
        std::filesystem::remove(packageFilePath);
        return -1;
    }

    package.seekg(0, std::ios::end);
    size_t packageSize = package.tellg();
    package.seekg(0, std::ios::beg);

    int result = processStream(package, packageSize);

    if (result != 0)
    {
        package.close();
        std::filesystem::remove(packageFilePath);
    }

    return result;
}

int UpdateManager::processStream(std::istream& package, size_t packageSize)
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
            return -1;
        }
        else
        {
            clearActivationInfo();
        }
    }
    package.seekg(0);
    std::vector<uint8_t> packageHeader(packageSize);
    package.read(reinterpret_cast<char*>(packageHeader.data()), packageSize);
    try
    {
        parser = std::make_unique<PackageParser>(packageHeader);
    }
    catch (const InternalFailure&)
    {
        error("Failed to parse the PLDM fw update package header");
        return -1;
    }
    if (parser == nullptr)
    {
        error("Invalid PLDM package header information");
        return -1;
    }

    // Populate object path with the hash of the package version
    size_t versionHash = std::hash<std::string>{}(parser->pkgVersion);
    if (!inventoryObjPath.empty())
    {
        objPath = inventoryObjPath;
    }
    else
    {
        objPath = swRootPath + std::to_string(versionHash);
    }

    package.seekg(0);
    packageHeader.resize(parser->pkgHeaderSize);
    package.read(reinterpret_cast<char*>(packageHeader.data()),
                 parser->pkgHeaderSize);
    auto deviceUpdaterInfos = associatePkgToDevices(
        parser->getFwDeviceIDRecords(), parser->getDownstreamDeviceIDRecords(),
        descriptorMap, downstreamDescriptorMap, totalNumComponentUpdates);
    if (!deviceUpdaterInfos.size())
    {
        error(
            "No matching devices found with the PLDM firmware update package");
        activation = std::make_unique<Activation>(
            pldm::utils::DBusHandler::getBus(), objPath,
            software::Activation::Activations::Invalid, this);
        parser.reset();
        return 0;
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
    if (!inventoryObjPath.empty())
    {
        activation->activation(software::Activation::Activations::Activating);
    }

    return 0;
}

DeviceUpdaterInfos UpdateManager::associatePkgToDevices(
    const FirmwareDeviceIDRecords& fwDeviceIDRecords,
    const DownstreamDeviceIDRecords& downstreamDeviceIDRecords,
    const DescriptorMap& descriptorMap,
    const DownstreamDescriptorMap& downstreamDescriptorMap,
    TotalComponentUpdates& totalNumComponentUpdates)
{
    DeviceUpdaterInfos deviceUpdaterInfos;
    for (size_t index = 0; index < fwDeviceIDRecords.size(); ++index)
    {
        const auto& deviceIDDescriptors =
            std::get<Descriptors>(fwDeviceIDRecords[index]);
        std::vector<Descriptor> deviceIDDescriptorsVec(
            deviceIDDescriptors.begin(), deviceIDDescriptors.end());
        std::sort(deviceIDDescriptorsVec.begin(), deviceIDDescriptorsVec.end());
        for (const auto& [eid, descriptors] : descriptorMap)
        {
            std::vector<Descriptor> descriptorsVec(descriptors.begin(),
                                                   descriptors.end());
            std::sort(descriptorsVec.begin(), descriptorsVec.end());
            if (std::includes(descriptorsVec.begin(), descriptorsVec.end(),
                              deviceIDDescriptorsVec.begin(),
                              deviceIDDescriptorsVec.end()))
            {
                deviceUpdaterInfos.emplace_back(std::make_pair(eid, index));
                const auto& applicableComponents =
                    std::get<ApplicableComponents>(fwDeviceIDRecords[index]);
                totalNumComponentUpdates += applicableComponents.size();
            }
        }
    }
    for (size_t index = 0; index < downstreamDeviceIDRecords.size(); ++index)
    {
        const auto& downstreamDeviceIDDescriptors =
            std::get<Descriptors>(downstreamDeviceIDRecords[index]);
        std::vector<Descriptor> downstreamDeviceIDDescriptorsVec(
            downstreamDeviceIDDescriptors.begin(),
            downstreamDeviceIDDescriptors.end());
        std::sort(downstreamDeviceIDDescriptorsVec.begin(),
                  downstreamDeviceIDDescriptorsVec.end());
        for (const auto& [eid, downstreamDeviceInfo] : downstreamDescriptorMap)
        {
            for (const auto& [downstreamDeviceIndex, descriptors] :
                 downstreamDeviceInfo)
            {
                std::vector<Descriptor> descriptorsVec(descriptors.begin(),
                                                       descriptors.end());
                std::sort(descriptorsVec.begin(), descriptorsVec.end());
                if (std::includes(descriptorsVec.begin(), descriptorsVec.end(),
                                  downstreamDeviceIDDescriptorsVec.begin(),
                                  downstreamDeviceIDDescriptorsVec.end()))
                {
                    deviceUpdaterInfos.emplace_back(std::make_pair(eid, index));
                    const auto& applicableComponents =
                        std::get<ApplicableComponents>(
                            downstreamDeviceIDRecords[index]);
                    totalNumComponentUpdates += applicableComponents.size();
                }
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

    deviceUpdaterMap.clear();
    deviceUpdateCompletionMap.clear();
    parser.reset();
    package.close();
    std::filesystem::remove(fwPackageFilePath);
    totalNumComponentUpdates = 0;
    compUpdateCompletedCount = 0;
}

void UpdateManager::updateActivationProgress()
{
    compUpdateCompletedCount++;
    auto progressPercent = static_cast<uint8_t>(std::floor(
        (100 * compUpdateCompletedCount) / totalNumComponentUpdates));
    activationProgress->progress(progressPercent);
}

} // namespace fw_update

} // namespace pldm
