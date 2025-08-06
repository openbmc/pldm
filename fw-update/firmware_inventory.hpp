#pragma once

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "device_updater.hpp"
#include "package_parser.hpp"
#include "requester/handler.hpp"

#include <libpldm/base.h>

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
#include <tuple>
#include <unordered_map>

namespace pldm::fw_update
{

class FirmwareInventory;
using SoftwareVersion = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;
using SoftwareAssociationDefinitions = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

using SoftwareVersionPurpose = SoftwareVersion::VersionPurpose;

using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm;
using Context = sdbusplus::async::context;

class FirmwareInventory
{
  public:
    FirmwareInventory() = delete;
    FirmwareInventory(const FirmwareInventory&) = delete;
    FirmwareInventory(FirmwareInventory&&) = delete;
    FirmwareInventory& operator=(const FirmwareInventory&) = delete;
    FirmwareInventory& operator=(FirmwareInventory&&) = delete;
    ~FirmwareInventory() = default;

    explicit FirmwareInventory(
        Event& /*event*/,
        pldm::requester::Handler<pldm::requester::Request>& /*handler*/,
        InstanceIdDb& instanceIdDb, pldm::eid eid,
        const std::string& softwarePath, const std::string& softwareVersion,
        const std::string& associatedEndpoint, const Descriptors& descriptors,
        const ComponentInfo& componentInfo,
        SoftwareVersionPurpose purpose = SoftwareVersionPurpose::Unknown);

    Response handleRequest(uint8_t command, const pldm_msg* request,
                           size_t reqMsgLen);

  private:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();

    [[maybe_unused]] pldm::eid eid;

    std::string softwarePath;

    std::unique_ptr<SoftwareAssociationDefinitions> association;

    std::unique_ptr<SoftwareVersion> version;

    [[maybe_unused]] const Descriptors& descriptors;

    ComponentInfo componentInfo;
};

} // namespace pldm::fw_update
