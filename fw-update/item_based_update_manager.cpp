#include "item_based_update_manager.hpp"

#include "activation.hpp"
#include "common/utils.hpp"
#include "package_parser.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cmath>
#include <span>
#include <spanstream>
#include <string>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

int ItemBasedUpdateManager::processPackage(std::span<const uint8_t> packageData)
{
    namespace software = sdbusplus::xyz::openbmc_project::Software::server;
    if (!activation ||
        activation->activation() != software::Activation::Activations::Active)
    {
        error("Activation is not in active state");
        return -1;
    }
    if (packageData.size() < sizeof(pldm_package_header_information))
    {
        error(
            "PLDM fw update package length {SIZE} less than the length of the package header information '{PACKAGE_HEADER_INFO_SIZE}'.",
            "SIZE", packageData.size(), "PACKAGE_HEADER_INFO_SIZE",
            sizeof(pldm_package_header_information));
        return -1;
    }

    std::vector<uint8_t> buffer(packageData.begin(), packageData.end());
    parser = parsePkgHeader(buffer);
    if (parser == nullptr)
    {
        error("Invalid PLDM package header information");
        return -1;
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
        return -1;
    }

    auto deviceIdRecordOffset =
        associatePkgToDevice(parser->getFwDeviceIDRecords(), descriptors);
    if (!deviceIdRecordOffset)
    {
        error("Failed to associate package to device");
        return -1;
    }

    const auto& fwDeviceIDRecords = parser->getFwDeviceIDRecords();
    const auto& compImageInfos = parser->getComponentImageInfos();

    packageDataStream = std::make_unique<std::ispanstream>(
        std::span<const char>(reinterpret_cast<const char*>(packageData.data()),
                              packageData.size()));
    deviceUpdater = std::make_unique<DeviceUpdater>(
        eid, *packageDataStream, fwDeviceIDRecords[*deviceIdRecordOffset],
        compImageInfos, componentInfo, MAXIMUM_TRANSFER_SIZE, this);
    activation->activation(software::Activation::Activations::Ready);
    activationProgress = std::make_unique<ActivationProgress>(
        pldm::utils::DBusHandler::getBus(), objPath);
    activation->activation(software::Activation::Activations::Activating);

    return 0; // Return appropriate status code
}

std::string ItemBasedUpdateManager::sendProcessPackage(
    std::span<const uint8_t> packageData)
{
    deferHandler = std::make_unique<sdeventplus::source::Defer>(
        event, [this, packageData](sdeventplus::source::EventBase&) {
            processPackage(packageData);
        });
    return objPath;
}

std::optional<DeviceIDRecordOffset>
    ItemBasedUpdateManager::associatePkgToDevice(
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

void ItemBasedUpdateManager::updateDeviceCompletion(mctp_eid_t /*eid*/,
                                                    bool /*status*/)
{
    progressPercentage = 100;
    updateActivationProgress();

    auto endTime = std::chrono::steady_clock::now();
    auto dur =
        std::chrono::duration<double, std::milli>(endTime - startTime).count();
    info("Firmware update time: {DURATION}ms", "DURATION", dur);
    activation->activation(software::Activation::Activations::Active);
    return;
}

Response ItemBasedUpdateManager::handleRequest(
    mctp_eid_t /*eid*/, uint8_t command, const pldm_msg* request,
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
            auto ret = deviceUpdater->transferComplete(request, reqMsgLen);
            progressPercentage = 33;
            return ret;
        }
        else if (command == PLDM_VERIFY_COMPLETE)
        {
            auto ret = deviceUpdater->verifyComplete(request, reqMsgLen);
            progressPercentage = 66;
            return ret;
        }
        else if (command == PLDM_APPLY_COMPLETE)
        {
            auto ret = deviceUpdater->applyComplete(request, reqMsgLen);
            progressPercentage = 99;
            return ret;
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

void ItemBasedUpdateManager::activatePackage()
{
    startTime = std::chrono::steady_clock::now();
    deviceUpdater->startFwUpdateFlow();
}

void ItemBasedUpdateManager::clearActivationInfo()
{
    activation->activation(software::Activation::Activations::Active);
    activationProgress.reset();
}

void ItemBasedUpdateManager::updateActivationProgress()
{
    activationProgress->progress(progressPercentage);
}

} // namespace pldm::fw_update
