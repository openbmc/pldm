#include "item_update_manager.hpp"

#include "activation.hpp"
#include "common/utils.hpp"
#include "package_parser.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <span>
#include <spanstream>
#include <string>
#include <system_error>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

bool ItemUpdateManager::processPackage()
{
    inProgressActivation = std::make_unique<Activation>(
        pldm::utils::DBusHandler::getBus(), objPathWithSwId,
        software::Activation::Activations::NotReady, this);

    if (packageMap->getSize() < sizeof(pldm_package_header_information))
    {
        error(
            "PLDM fw update package length {SIZE} less than the length of the package header information '{PACKAGE_HEADER_INFO_SIZE}'.",
            "SIZE", packageMap->getSize(), "PACKAGE_HEADER_INFO_SIZE",
            sizeof(pldm_package_header_information));
        packageMap.reset();
        return false;
    }

    constexpr size_t packageHeaderSizeOffset = 17;
    uint16_t* packageHeaderSize = std::bit_cast<uint16_t*>(
        packageMap->getData() + packageHeaderSizeOffset);

    auto buffer = std::vector<uint8_t>(
        packageMap->getBytes().begin(),
        packageMap->getBytes().begin() + le16toh(*packageHeaderSize));
    parser = parsePkgHeader(buffer);
    if (parser == nullptr)
    {
        error("Invalid PLDM package header information");
        packageMap.reset();
        return false;
    }
    try
    {
        parser->parse(buffer, buffer.size());
    }
    catch (const std::exception& e)
    {
        error("Invalid PLDM package header, error - {ERROR}", "ERROR", e);
        inProgressActivation->activation(
            software::Activation::Activations::Invalid);
        parser.reset();
        packageMap.reset();
        return false;
    }

    auto deviceIdRecordOffset =
        associatePkgToDevice(parser->getFwDeviceIDRecords(), descriptors);
    if (!deviceIdRecordOffset)
    {
        error("Failed to associate package to device");
        packageMap.reset();
        return false;
    }

    const auto& fwDeviceIDRecords = parser->getFwDeviceIDRecords();
    const auto& compImageInfos = parser->getComponentImageInfos();
    static constexpr uint32_t MAXIMUM_TRANSFER_SIZE = 4096;

    auto packageSpan = packageMap->getChars();
    packageDataStream =
        std::make_unique<std::ispanstream>(packageSpan, std::ios::binary);
    deviceUpdater = std::make_unique<DeviceUpdater>(
        eid, *packageDataStream, fwDeviceIDRecords[*deviceIdRecordOffset],
        compImageInfos, componentInfo, MAXIMUM_TRANSFER_SIZE, this);
    inProgressActivation->activation(software::Activation::Activations::Ready);
    activationProgress = std::make_unique<ActivationProgress>(
        pldm::utils::DBusHandler::getBus(), objPathWithSwId);
    inProgressActivation->activation(
        software::Activation::Activations::Activating);

    return true;
}

std::string ItemUpdateManager::processFd(int fd)
{
    objPathWithSwId = std::format("{}_{}", objPath, utils::generateSwId());
    auto rawDupFd = dup(fd);
    if (rawDupFd < 0)
    {
        error("Failed to duplicate package file descriptor");
        throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
    }
    this->dupFd = std::make_unique<pldm::utils::CustomFD>(rawDupFd);
    deferHandler = std::make_unique<
        sdeventplus::source::Defer>(event, [this](sdeventplus::source::
                                                      EventBase&) {
        try
        {
            packageMap =
                std::make_unique<pldm::utils::MMapHandler>((*this->dupFd)());
        }
        catch (const std::system_error& e)
        {
            error("Failed to mmap package file, error - {ERROR}", "ERROR", e);
            updateInProgress = false;
            this->dupFd.reset();
            throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
        }
        if (!processPackage())
        {
            error("Failed to process firmware update package");
            updateInProgress = false;
            packageMap.reset();
            this->dupFd.reset();
            throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
        }
    });
    return objPathWithSwId;
}

std::optional<DeviceIDRecordOffset> ItemUpdateManager::associatePkgToDevice(
    const FirmwareDeviceIDRecords& fwDeviceIDRecords,
    const Descriptors& descriptors)
{
    for (size_t index = 0; index < fwDeviceIDRecords.size(); ++index)
    {
        const auto& deviceIDDescriptors =
            std::get<Descriptors>(fwDeviceIDRecords[index]);
        if (std::includes(descriptors.begin(), descriptors.end(),
                          deviceIDDescriptors.begin(),
                          deviceIDDescriptors.end()))
        {
            return index;
        }
    }
    return std::nullopt;
}

void ItemUpdateManager::updateDeviceCompletion(mctp_eid_t /*eid*/, bool status)
{
    activationProgress->progress(100);
    packageMap.reset();
    dupFd.reset();

    auto endTime = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration<double, std::milli>(endTime - startTime).count();
    info("Firmware update time: {DURATION}ms", "DURATION", dur);
    activationProgress.reset();
    inProgressActivation->activation(
        status ? software::Activation::Activations::Active
               : software::Activation::Activations::Failed);
    deviceUpdater.reset();
    packageDataStream.reset();
    packageMap.reset();
    dupFd.reset();
    updateInProgress = false;
    return;
}

Response ItemUpdateManager::handleRequest(mctp_eid_t /*eid*/, uint8_t command,
                                          const pldm_msg* request,
                                          size_t reqMsgLen)
{
    Response response(sizeof(pldm_msg), 0);
    if (deviceUpdater)
    {
        if (command == PLDM_REQUEST_FIRMWARE_DATA)
        {
            return deviceUpdater->requestFwData(request, reqMsgLen);
        }
        else if (command == PLDM_TRANSFER_COMPLETE)
        {
            return deviceUpdater->transferComplete(request, reqMsgLen);
        }
        else if (command == PLDM_VERIFY_COMPLETE)
        {
            return deviceUpdater->verifyComplete(request, reqMsgLen);
        }
        else if (command == PLDM_APPLY_COMPLETE)
        {
            return deviceUpdater->applyComplete(request, reqMsgLen);
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

void ItemUpdateManager::activatePackage()
{
    startTime = std::chrono::steady_clock::now();
    deviceUpdater->startFwUpdateFlow();
}

void ItemUpdateManager::resetActivationState()
{
    inProgressActivation.reset();
    activationProgress.reset();
    dupFd.reset();
    updateInProgress = false;
}

void ItemUpdateManager::updateActivationProgress()
{
    // TODO: implement in the next patch
}

sdbusplus::message::object_path ItemUpdateManager::startUpdate(
    sdbusplus::message::unix_fd image,
    ApplyTimeIntf::RequestedApplyTimes /*applyTime*/)
{
    if (updateInProgress)
    {
        error("Update already in progress");
        throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
    }
    if (image.fd < 0)
    {
        error("Invalid package file descriptor");
        throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
    }
    updateInProgress = true;

    return processFd(image.fd);
}

} // namespace pldm::fw_update
