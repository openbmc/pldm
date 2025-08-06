#include "manager.hpp"

#include "device.hpp"
#include "device_common.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>
#include <xyz/openbmc_project/PLDM/Event/server.hpp>

#include <future>
#include <iomanip>
#include <iostream>

PHOSPHOR_LOG2_USING;

namespace pldm::rde
{
Manager::Manager(sdbusplus::bus::bus& bus, sdeventplus::Event& event,
                 pldm::InstanceIdDb* instanceIdDb,
                 pldm::requester::Handler<pldm::requester::Request>* handler) :
    sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::RDE::server::Manager>(
        bus, std::string(RDEManagerObjectPath).c_str()),
    instanceIdDb_(instanceIdDb), handler_(handler), bus_(bus), event_(event)
{}

void Manager::handleMctpEndpoints(const std::vector<MctpInfo>& mctpInfos)
{
    for (const auto& mctpInfo : mctpInfos)
    {
        const eid devEID = std::get<0>(mctpInfo);
        const UUID& devUUID = std::get<1>(mctpInfo);

        info("RDE: Handling device UUID:{UUID} EID:{EID}", "UUID", devUUID,
             "EID", static_cast<int>(devEID));

        // Skip if already registered
        if (signalMatches_.count(devEID))
            continue;

        auto match = std::make_unique<sdbusplus::bus::match_t>(
            bus_,
            sdbusplus::bus::match::rules::type::signal() +
                sdbusplus::bus::match::rules::member("DiscoveryComplete") +
                sdbusplus::bus::match::rules::interface(
                    "xyz.openbmc_project.PLDM.Event") +
                sdbusplus::bus::match::rules::path("/xyz/openbmc_project/pldm"),
            [this, devEID, devUUID](sdbusplus::message::message& msg) {
                uint8_t signalTid = 0;
                std::vector<std::vector<uint8_t>> pdrPayloads;
                msg.read(signalTid, pdrPayloads);

                info("RDE: Call back device UUID:{UUID} EID:{EID} TID: {TID}",
                     "UUID", devUUID, "EID", static_cast<int>(devEID), "TID",
                     static_cast<int>(signalTid));

                if (!eidMap_.count(devEID))
                {
                    this->createDeviceDbusObject(devEID, devUUID, signalTid,
                                                 pdrPayloads);

                    // Remove match if one-time use
                    signalMatches_.erase(devEID);
                }
            });

        signalMatches_[devEID] = std::move(match);
    }
}

void Manager::createDeviceDbusObject(
    eid devEID, const UUID& devUUID, pldm_tid_t tid,
    const std::vector<std::vector<uint8_t>>& pdrPayloads)
{
    // Prevent duplicate creation for the same EID
    if (eidMap_.count(eid()))
    {
        info("Device for EID already exists. Skipping registration.");
        return;
    }

    std::string path =
        std::string(DeviceObjectPath) + "/" + std::to_string(devEID);
    std::string friendlyName = "Device_" + std::to_string(devEID);

    // Create base device
    auto devicePtr =
        std::make_shared<Device>(bus_, event_, path, instanceIdDb_, handler_,
                                 devEID, tid, devUUID, pdrPayloads);
    bus_.request_name(DeviceServiceName.data());

    DeviceContext context;
    context.uuid = devUUID;
    context.deviceEID = devEID;
    context.tid = tid;
    context.friendlyName = friendlyName;
    context.devicePtr = devicePtr;

    info("RDE device created UUID:{UUID} EID:{EID} Path:{PATH}, Name:{NAME}",
         "UUID", devUUID, "EID", static_cast<int>(devEID), "PATH", path, "NAME",
         friendlyName);

    eidMap_[devEID] = std::move(context);

    devicePtr->refreshDeviceInfo();
}

DeviceContext* Manager::getDeviceContext(eid devEID)
{
    auto it = eidMap_.find(devEID);
    if (it != eidMap_.end())
    {
        return &(it->second);
    }
    return nullptr;
}

ObjectPath Manager::startRedfishOperation(
    uint32_t operationID,
    sdbusplus::common::xyz::openbmc_project::rde::Common::OperationType
        operationType,
    std::string targetURI, std::string deviceUUID, uint8_t eid,
    std::string payload, PayloadFormatType payloadFormat,
    EncodingFormatType encodingFormat, std::string sessionId)
{
    info("RDE startRedfishOperation: UUID={UUID} EID={EID}", "UUID", deviceUUID,
         "EID", static_cast<int>(eid));

    auto it = eidMap_.find(eid);
    if (it == eidMap_.end() || !it->second.devicePtr)
    {
        error("RDE: No valid device context found for EID={EID}", "EID",
              static_cast<int>(eid));
        return ObjectPath{""};
    }

    debug("RDE: devicePtr use_count={COUNT} address={ADDR}", "COUNT",
          it->second.devicePtr.use_count(), "ADDR",
          static_cast<const void*>(it->second.devicePtr.get()));

    // Build D-Bus object path
    std::string taskPathStr =
        "/xyz/openbmc_project/RDE/OperationTask/" + std::to_string(operationID);
    ObjectPath objPath{taskPathStr};

    // Create and register D-Bus OperationTask object
    auto task = std::make_shared<OperationTask>(bus_, taskPathStr);
    // task->emitInterfaceAdded();

    taskMap_[operationID] = task;

    // Construct minimal OperationInfo
    OperationInfo opInitInfo{
        operationID, operationType, targetURI,      deviceUUID, eid,
        payload,     payloadFormat, encodingFormat, sessionId,  taskPathStr};

    // Launch the Redfish operation
    it->second.devicePtr->performRDEOperation(opInitInfo);

    return objPath;
}

SchemaResourcesType Manager::getDeviceSchemaInfo(std::string deviceUUID)
{
    for (const auto& [eid, context] : eidMap_)
    {
        if (context.uuid == deviceUUID && context.devicePtr)
        {
            return context.devicePtr->schemaResources();
        }
    }

    // If no match found
    return {};
}

std::vector<OperationType> Manager::getSupportedOperations(
    std::string /*deviceUUID*/)
{
    // TODO: Implement supported operations retrieval
    return {};
}

} // namespace pldm::rde
