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

PHOSPHOR_LOG2_USING;

namespace pldm
{

namespace fw_update
{

namespace software = sdbusplus::xyz::openbmc_project::Software::server;

int UpdateManager::processFd(int fd)
{
   sstream = std::stringstream(std::ios::in|std::ios::out|std::ios::binary);
    if(!sstream)
    {
        error("Invalid stringstream");
        return -1;
    }

    const int BUFFER_SIZE = 0x200;
    ssize_t bytesRead;
    char buffer[BUFFER_SIZE];
    // Read from source and write to destination
    while ((bytesRead = read(fd, buffer, BUFFER_SIZE)) > 0)
    {
        sstream.write(buffer,bytesRead);
    }
    sstream.seekg(0);
    sstream.seekp(0);
    if (bytesRead < 0)
    {
        std::cerr << "Failed to read from source file: " << strerror(errno)
                  << std::endl;
        return -1;
    }

    return processIstream(sstream);
}

int UpdateManager::processIstream(std::istream& stream)
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
    stream.seekg(0,std::ios::end);
    uintmax_t packageSize = stream.tellg();
    if (packageSize < sizeof(pldm_package_header_information))
    {
        error(
            "PLDM fw update package length {SIZE} less than the length of the package header information '{PACKAGE_HEADER_INFO_SIZE}'.",
            "SIZE", packageSize, "PACKAGE_HEADER_INFO_SIZE",
            sizeof(pldm_package_header_information));
        return -1;
    }
    stream.seekg(0);
    std::vector<uint8_t> packageHeader(sizeof(pldm_package_header_information));
    stream.read(reinterpret_cast<char*>(packageHeader.data()),
                 sizeof(pldm_package_header_information));

    auto pkgHeaderInfo =
        reinterpret_cast<const pldm_package_header_information*>(
            packageHeader.data());
    auto pkgHeaderInfoSize = sizeof(pldm_package_header_information) +
                             pkgHeaderInfo->package_version_string_length;
    packageHeader.clear();
    packageHeader.resize(pkgHeaderInfoSize);
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(packageHeader.data()),
                 pkgHeaderInfoSize);
    parser = parsePkgHeader(packageHeader);
    if (parser == nullptr)
    {
        error("Invalid PLDM package header information");
        return -1;
    }

    // Populate object path with the hash of the package version
    size_t versionHash = std::hash<std::string>{}(parser->pkgVersion);
    if(objPath.empty())
    {
        if(_overrideObjPath.empty())
            objPath = swRootPath + std::to_string(versionHash);
        else
            objPath = _overrideObjPath;
    }

    stream.seekg(0);
    packageHeader.resize(parser->pkgHeaderSize);
    stream.read(reinterpret_cast<char*>(packageHeader.data()),
                 parser->pkgHeaderSize);
    try
    {
        parser->parse(packageHeader, packageSize);
    }
    catch (const std::exception& e)
    {
        error("Invalid PLDM package header, error - {ERROR}", "ERROR", e);
        if(activation)
            activation->activation(software::Activation::Activations::Invalid);
        else
        {
            activation = std::make_shared<Activation>(
                pldm::utils::DBusHandler::getBus(), objPath,
                software::Activation::Activations::Invalid, this);
        }
        parser.reset();
        return -1;
    }
    auto deviceUpdaterInfos =
        associatePkgToDevices(parser->getFwDeviceIDRecords(), descriptorMap,
                              totalNumComponentUpdates);
    if (!deviceUpdaterInfos.size())
    {
        error(
            "No matching devices found with the PLDM firmware update package");
        if(activation)
            activation->activation(software::Activation::Activations::Invalid);
        else
        {
            activation = std::make_shared<Activation>(
                pldm::utils::DBusHandler::getBus(), objPath,
                software::Activation::Activations::Invalid, this);
        }
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
                deviceUpdaterInfo.first, stream, fwDeviceIDRecord,
                compImageInfos, search->second, MAXIMUM_TRANSFER_SIZE, this));
    }
    if(activation)
        activation->activation(software::Activation::Activations::Ready);
    else
    {
        info("no activation, objPath is {OBJ_PATH}","OBJ_PATH",objPath);
        activation = std::make_shared<Activation>(
            pldm::utils::DBusHandler::getBus(), objPath,
            software::Activation::Activations::Ready, this);
    }
    activationProgress = std::make_shared<ActivationProgress>(
        pldm::utils::DBusHandler::getBus(), objPath);
    activation->activation(software::Activation::Activations::Activating);
    return 0;
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

void UpdateManager::assignActivation(std::shared_ptr<Activation> activation)
{
    this->activation = activation;
}

void UpdateManager::overrideObjPath(const std::string& path)
{
    objPath = _overrideObjPath = path;
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
            auto ptr = reinterpret_cast<pldm_msg*>(response.data());
            auto rc = encode_cc_only_resp(
                request->hdr.instance_id, request->hdr.type,
                request->hdr.command, PLDM_ERROR_INVALID_DATA, ptr);
            assert(rc == PLDM_SUCCESS);
        }
    }
    else
    {
        auto ptr = reinterpret_cast<pldm_msg*>(response.data());
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
