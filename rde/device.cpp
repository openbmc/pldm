#include "device.hpp"

#include <filesystem>
#include <iostream>

namespace pldm::rde
{
Device::Device(sdbusplus::bus::bus& bus, sdeventplus::Event& event,
               const std::string& path, pldm::InstanceIdDb* instanceIdDb,
               pldm::requester::Handler<pldm::requester::Request>* handler,
               const pldm::eid devEid, const pldm_tid_t tid,
               const pldm::UUID& uuid,
               const std::vector<std::vector<uint8_t>>& pdrPayloads) :
    EntryIfaces(bus, path.c_str()), instanceIdDb_(instanceIdDb),
    handler_(handler), event_(event), tid_(tid), pdrPayloads_(pdrPayloads),
    currentState_(DeviceState::NotReady)
{
    info(
        "RDE : device Object creating device UUID:{UUID} EID:{EID} Path:{PATH}",
        "UUID", uuid, "EID", static_cast<int>(devEid), "PATH", path);

    this->eid(devEid);
    this->deviceUUID(uuid);
    this->name("Device_" + std::to_string(devEid));
    this->negotiationStatus(NegotiationStatus::NotStarted);
}

Device::~Device()
{
    info("RDE: D-Bus Device object destroyed");
}

void Device::refreshDeviceInfo()
{
    info("RDE : Refreshing device EID:{EID}", "EID", static_cast<int>(eid()));

    try
    {
        resourceRegistry_ = std::make_unique<ResourceRegistry>();
        resourceRegistry_->loadFromResourcePDR(pdrPayloads_);
        schemaResources(buildSchemaResourcesPayload());

        dictionaryManager_ =
            std::make_unique<pldm::rde::DictionaryManager>(deviceUUID());

        std::shared_ptr<Device> self;
        try
        {
            self = shared_from_this();
        }
        catch (const std::bad_weak_ptr& e)
        {
            error("Device shared_from_this() failed: Msg={MSG}", "MSG",
                  e.what());
            return;
        }

        info("RDE: Discovery sequence started for EID={EID}, use_count={COUNT}",
             "EID", static_cast<int>(eid()), "COUNT", self.use_count());

        discovSession_ = std::make_unique<DiscoverySession>(self);
        discovSession_->doNegotiateRedfish();
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        error("refreshDeviceInfo D-Bus error: Msg={MSG}", "MSG", e.what());
        return;
    }
    catch (const std::exception& e)
    {
        error("refreshDeviceInfo: Failed : Msg={MSG}", "MSG", e.what());
        return;
    }

    std::map<std::string,
             std::variant<std::string, uint16_t, uint32_t, uint8_t>>
        changed;
    changed["Name"] = name(); // Simplified access

    deviceUpdated();
}

Metadata& Device::getMetadata()
{
    return metaData_;
}

void Device::setMetadata(const Metadata& meta)
{
    metaData_ = meta;
}

MetadataVariant Device::getMetadataField(const std::string& key) const
{
    if (key == "devProviderName")
        return metaData_.devProviderName;
    if (key == "etag")
        return metaData_.etag;
    if (key == "devConfigSignature")
        return metaData_.devConfigSignature;
    if (key == "mcMaxTransferChunkSizeBytes")
        return metaData_.mcMaxTransferChunkSizeBytes;
    if (key == "devMaxTransferChunkSizeBytes")
        return metaData_.devMaxTransferChunkSizeBytes;
    if (key == "mcConcurrencySupport")
        return metaData_.mcConcurrencySupport;
    if (key == "deviceConcurrencySupport")
        return metaData_.deviceConcurrencySupport;
    if (key == "protocolVersion")
        return metaData_.protocolVersion;
    if (key == "encoding")
        return metaData_.encoding;
    if (key == "sessionId")
        return metaData_.sessionId;
    if (key == "mcFeatureSupport")
        return metaData_.mcFeatureSupport;
    if (key == "devFeatureSupport")
        return metaData_.devFeatureSupport;
    if (key == "devCapabilities")
        return metaData_.devCapabilities;
    return std::string{};
}

void Device::setMetadataField(const std::string& key,
                              const MetadataVariant& value)
{
    try
    {
        if (key == "devProviderName")
            metaData_.devProviderName = std::get<std::string>(value);
        else if (key == "etag")
            metaData_.etag = std::get<std::string>(value);
        else if (key == "devConfigSignature")
            metaData_.devConfigSignature = std::get<uint32_t>(value);
        else if (key == "mcMaxTransferChunkSizeBytes")
            metaData_.mcMaxTransferChunkSizeBytes = std::get<uint32_t>(value);
        else if (key == "devMaxTransferChunkSizeBytes")
            metaData_.devMaxTransferChunkSizeBytes = std::get<uint32_t>(value);
        else if (key == "deviceConcurrencySupport")
            metaData_.deviceConcurrencySupport = std::get<uint8_t>(value);
        else if (key == "mcConcurrencySupport")
            metaData_.mcConcurrencySupport = std::get<uint8_t>(value);
        else if (key == "protocolVersion")
            metaData_.protocolVersion = std::get<std::string>(value);
        else if (key == "encoding")
            metaData_.encoding = std::get<std::string>(value);
        else if (key == "sessionId")
            metaData_.sessionId = std::get<std::string>(value);
        else if (key == "mcFeatureSupport")
            metaData_.mcFeatureSupport = std::get<FeatureSupport>(value);
        else if (key == "devFeatureSupport")
            metaData_.devFeatureSupport = std::get<FeatureSupport>(value);
        else if (key == "devCapabilities")
            metaData_.devCapabilities = std::get<DeviceCapabilities>(value);
        else
            error("Unknown metadata key:{KEY}", "KEY", key);
    }
    catch (const std::bad_variant_access& ex)
    {
        error("Metadata type mismatch for key {KEY}: {WHAT}", "KEY", key,
              "WHAT", ex.what());
    }
    catch (const std::exception& ex)
    {
        error("Failed to set metadata key {KEY}: {WHAT}", "KEY", key, "WHAT",
              ex.what());
    }
}

DeviceState Device::getState() const
{
    return currentState_;
}

void Device::updateState(DeviceState newState)
{
    currentState_ = newState;
}

SchemaResourcesType Device::buildSchemaResourcesPayload() const
{
    SchemaResourcesType payload;

    if (!resourceRegistry_)
        return payload;

    const auto& resourceMap = resourceRegistry_->getResourceMap();

    for (const auto& [resourceId, info] : resourceMap)
    {
        PropertyMap entry;

        // Canonical schema keys from SchemaResourceKeys definition
        entry["subUri"] = info.uri;
        entry["schemaName"] = info.schemaName;
        entry["schemaVersion"] = info.schemaVersion;
        entry["schemaClass"] = static_cast<int64_t>(info.schemaClass);
        entry["ProposedContainingResourceName"] = info.propContainResourceName;
        entry["operations"] =
            static_cast<int64_t>(info.operations.size()); // Count for now

        // Optional/OEM extensions: placeholders (can be populated when
        // available)
        entry["OEMExtensions"] =
            std::string{}; // Replace with actual logic if applicable
        entry["Actions"] =
            std::string{}; // Replace with action serialization when added
        entry["ActionName"] =
            std::string{}; // Optional if action metadata is tracked
        entry["ActionPath"] = std::string{}; // Optional action URI

        payload[resourceId] = std::move(entry);
    }

    return payload;
}

} // namespace pldm::rde
