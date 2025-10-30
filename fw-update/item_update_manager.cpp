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

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

bool ItemUpdateManager::processPackage()
{
    if (packageMap->size() < sizeof(pldm_package_header_information))
    {
        error(
            "PLDM fw update package length {SIZE} less than the length of the package header information '{PACKAGE_HEADER_INFO_SIZE}'.",
            "SIZE", packageMap->size(), "PACKAGE_HEADER_INFO_SIZE",
            sizeof(pldm_package_header_information));
        packageMap.reset();
        return false;
    }

    auto buffer = packageMap->toVector<uint8_t>();
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
        activation = std::make_unique<Activation>(
            pldm::utils::DBusHandler::getBus(), objPath,
            software::Activation::Activations::Invalid, this);
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

    auto packageSpan = packageMap->getSpan<char>();
    packageDataStream =
        std::make_unique<std::ispanstream>(packageSpan, std::ios::binary);
    deviceUpdater = std::make_unique<DeviceUpdater>(
        eid, *packageDataStream, fwDeviceIDRecords[*deviceIdRecordOffset],
        compImageInfos, componentInfo, MAXIMUM_TRANSFER_SIZE, this);
    activation->activation(software::Activation::Activations::Ready);
    activationProgress = std::make_unique<ActivationProgress>(
        pldm::utils::DBusHandler::getBus(), objPath);
    activation->activation(software::Activation::Activations::Activating);

    return true;
}

std::string ItemUpdateManager::processFd(int fd)
{
    auto dupFd = dup(fd);
    if (dupFd < 0)
    {
        error("Failed to duplicate package file descriptor");
        throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
    }
    deferHandler = std::make_unique<
        sdeventplus::source::Defer>(event, [this, dupFd](sdeventplus::source::
                                                             EventBase&) {
        packageMap = std::make_unique<pldm::utils::MMapHandler>(dupFd);
        if (packageMap->data() == nullptr)
        {
            error("Failed to mmap package file");
            throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
        }
        if (!processPackage())
        {
            error("Failed to process firmware update package");
            throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
        }
    });
    return objPath;
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

    auto endTime = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration<double, std::milli>(endTime - startTime).count();
    info("Firmware update time: {DURATION}ms", "DURATION", dur);
    activationProgress.reset();
    activation->activation(status ? software::Activation::Activations::Active
                                  : software::Activation::Activations::Failed);
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
    activation->activation(software::Activation::Activations::Active);
    activationProgress.reset();
}

void ItemUpdateManager::updateActivationProgress()
{
    // TODO: implement in the next patch
}

sdbusplus::message::object_path ItemUpdateManager::startUpdate(
    sdbusplus::message::unix_fd image,
    ApplyTimeIntf::RequestedApplyTimes /*applyTime*/)
{
    info("Starting update process with FD {FD}", "FD", image.fd);
    if (!activation ||
        activation->activation() != software::Activation::Activations::Active)
    {
        error("Activation is not in active state");
        throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
    }
    if (image.fd < 0)
    {
        error("Invalid package file descriptor");
        throw sdbusplus::xyz::openbmc_project::Common::Error::Unavailable();
    }

    return processFd(image.fd);
}

} // namespace pldm::fw_update
