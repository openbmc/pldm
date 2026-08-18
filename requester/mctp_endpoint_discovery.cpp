#include "mctp_endpoint_discovery.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <linux/mctp.h>

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace sdbusplus::match_rules;

PHOSPHOR_LOG2_USING;

namespace pldm
{
MctpDiscovery::MctpDiscovery(
    sdbusplus::bus_t& bus,
    std::initializer_list<MctpDiscoveryHandlerIntf*> list,
    sdeventplus::Event& event) :
    bus(bus),
    mctpEndpointAddedSignal(
        bus, interfacesAdded(MCTPPath),
        [this](sdbusplus::message_t& msg) { this->discoverEndpoints(msg); }),
    mctpEndpointRemovedSignal(
        bus, interfacesRemoved(MCTPPath),
        [this](sdbusplus::message_t& msg) { this->removeEndpoints(msg); }),
    mctpEndpointPropChangedSignal(
        bus, propertiesChangedNamespace(MCTPPath, MCTPInterfaceCC),
        [this](sdbusplus::message_t& msg) { this->propertiesChangedCb(msg); }),
    handlers(list),
    bridgeLearnTimer(event,
                     std::bind(std::mem_fn(&MctpDiscovery::retryBridgeLearn),
                               this))
{
    std::map<MctpInfo, Availability> currentMctpInfoMap;
    getMctpInfos(currentMctpInfoMap);
    for (const auto& mapIt : currentMctpInfoMap)
    {
        if (mapIt.second)
        {
            // Only add the available endpoints to the terminus
            // Let the propertiesChanged signal tells us when it comes back
            // to Available again
            addToExistingMctpInfos(MctpInfos(1, mapIt.first));
        }
    }
    handleMctpEndpoints(existingMctpInfos);
}

void MctpDiscovery::getMctpInfos(std::map<MctpInfo, Availability>& mctpInfoMap)
{
    // Find all implementations of the MCTP Endpoint interface
    pldm::utils::GetSubTreeResponse mapperResponse;
    try
    {
        mapperResponse = pldm::utils::DBusHandler().getSubtree(
            MCTPPath, 0, std::vector<std::string>({MCTPEndpoint::interface}));
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Failed to getSubtree call at path '{PATH}' and interface '{INTERFACE}', error - {ERROR} ",
            "ERROR", e, "PATH", MCTPPath, "INTERFACE", MCTPEndpoint::interface);
        return;
    }

    for (const auto& [path, services] : mapperResponse)
    {
        for (const auto& serviceIter : services)
        {
            const std::string& service = serviceIter.first;
            const MctpEndpointProps& epProps =
                getMctpEndpointProps(service, path);
            const UUID& uuid = getEndpointUUIDProp(service, path);
            const Availability& availability =
                getEndpointConnectivityProp(path);
            auto types = std::get<MCTPMsgTypes>(epProps);
            if (std::find(types.begin(), types.end(), mctpTypePLDM) !=
                types.end())
            {
                auto mctpInfo = MctpInfo(std::get<eid>(epProps), uuid, "",
                                         std::get<NetworkId>(epProps),
                                         std::nullopt, std::nullopt);
                searchConfigurationFor(pldm::utils::DBusHandler(), mctpInfo);
                mctpInfoMap[std::move(mctpInfo)] = availability;
            }
        }
    }
}

MctpEndpointProps MctpDiscovery::getMctpEndpointProps(
    const std::string& service, const std::string& path)
{
    try
    {
        auto properties = pldm::utils::DBusHandler().getDbusPropertiesVariant(
            service.c_str(), path.c_str(), MCTPEndpoint::interface);

        if (properties.contains(MCTPEndpoint::property_names::network_id) &&
            properties.contains(MCTPEndpoint::property_names::eid) &&
            properties.contains(
                MCTPEndpoint::property_names::supported_message_types))
        {
            auto networkId = std::get<NetworkId>(
                properties.at(MCTPEndpoint::property_names::network_id));
            auto eid = std::get<mctp_eid_t>(
                properties.at(MCTPEndpoint::property_names::eid));
            auto types = std::get<std::vector<uint8_t>>(properties.at(
                MCTPEndpoint::property_names::supported_message_types));
            return MctpEndpointProps(networkId, eid, types);
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Error reading MCTP Endpoint property at path '{PATH}' and service '{SERVICE}', error - {ERROR}",
            "SERVICE", service, "PATH", path, "ERROR", e);
        return MctpEndpointProps(0, MCTP_ADDR_ANY, {});
    }

    return MctpEndpointProps(0, MCTP_ADDR_ANY, {});
}

UUID MctpDiscovery::getEndpointUUIDProp(const std::string& service,
                                        const std::string& path)
{
    try
    {
        auto properties = pldm::utils::DBusHandler().getDbusPropertiesVariant(
            service.c_str(), path.c_str(), CommonUUID::interface);

        if (properties.contains("UUID"))
        {
            return std::get<UUID>(properties.at("UUID"));
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Error reading Endpoint UUID property at path '{PATH}' and service '{SERVICE}', error - {ERROR}",
            "SERVICE", service, "PATH", path, "ERROR", e);
        return static_cast<UUID>(emptyUUID);
    }

    return static_cast<UUID>(emptyUUID);
}

Availability MctpDiscovery::getEndpointConnectivityProp(const std::string& path)
{
    Availability available = false;
    try
    {
        pldm::utils::PropertyValue propertyValue =
            pldm::utils::DBusHandler().getDbusPropertyVariant(
                path.c_str(), MCTPConnectivityProp, MCTPInterfaceCC);
        if (std::get<std::string>(propertyValue) == "Available")
        {
            available = true;
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Error reading Endpoint Connectivity property at path '{PATH}', error - {ERROR}",
            "PATH", path, "ERROR", e);
    }

    return available;
}

void MctpDiscovery::getAddedMctpInfos(sdbusplus::message_t& msg,
                                      MctpInfos& mctpInfos)
{
    using ObjectPath = sdbusplus::object_path;
    ObjectPath objPath;
    using Property = std::string;
    using PropertyMap = std::map<Property, dbus::Value>;
    std::map<std::string, PropertyMap> interfaces;
    std::string uuid = emptyUUID;

    try
    {
        msg.read(objPath, interfaces);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Error reading MCTP Endpoint added interface message, error - {ERROR}",
            "ERROR", e);
        return;
    }
    Availability availability = getEndpointConnectivityProp(objPath.str);

    /* Get UUID */
    try
    {
        auto service = pldm::utils::DBusHandler().getService(
            objPath.str.c_str(), CommonUUID::interface);
        uuid = getEndpointUUIDProp(service, objPath.str);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Error getting Endpoint UUID D-Bus interface, error - {ERROR}",
              "ERROR", e);
    }

    for (const auto& [intfName, properties] : interfaces)
    {
        if (intfName == MCTPEndpoint::interface)
        {
            if (properties.contains(MCTPEndpoint::property_names::network_id) &&
                properties.contains(MCTPEndpoint::property_names::eid) &&
                properties.contains(
                    MCTPEndpoint::property_names::supported_message_types))
            {
                auto networkId = std::get<NetworkId>(
                    properties.at(MCTPEndpoint::property_names::network_id));
                auto eid = std::get<mctp_eid_t>(
                    properties.at(MCTPEndpoint::property_names::eid));
                auto types = std::get<std::vector<uint8_t>>(properties.at(
                    MCTPEndpoint::property_names::supported_message_types));

                if (!availability)
                {
                    // Log an error message here, but still add it to the
                    // terminus
                    error(
                        "mctpd added a DEGRADED endpoint {EID} networkId {NET} to D-Bus",
                        "NET", networkId, "EID", static_cast<unsigned>(eid));
                }
                if (std::find(types.begin(), types.end(), mctpTypePLDM) !=
                    types.end())
                {
                    info(
                        "Adding Endpoint networkId '{NETWORK}' and EID '{EID}' UUID '{UUID}'",
                        "NETWORK", networkId, "EID", eid, "UUID", uuid);
                    auto mctpInfo = MctpInfo(eid, uuid, "", networkId,
                                             std::nullopt, std::nullopt);
                    searchConfigurationFor(pldm::utils::DBusHandler(),
                                           mctpInfo);
                    mctpInfos.emplace_back(std::move(mctpInfo));
                }
            }
        }
    }
}

void MctpDiscovery::addToExistingMctpInfos(const MctpInfos& addedInfos)
{
    for (const auto& mctpInfo : addedInfos)
    {
        if (std::find(existingMctpInfos.begin(), existingMctpInfos.end(),
                      mctpInfo) == existingMctpInfos.end())
        {
            existingMctpInfos.emplace_back(mctpInfo);
        }
    }
}

void MctpDiscovery::removeFromExistingMctpInfos(MctpInfos& mctpInfos,
                                                MctpInfos& removedInfos)
{
    for (const auto& mctpInfo : existingMctpInfos)
    {
        if (std::find(mctpInfos.begin(), mctpInfos.end(), mctpInfo) ==
            mctpInfos.end())
        {
            removedInfos.emplace_back(mctpInfo);
        }
    }
    for (const auto& mctpInfo : removedInfos)
    {
        info("Removing Endpoint networkId '{NETWORK}' and  EID '{EID}'",
             "NETWORK", std::get<3>(mctpInfo), "EID", std::get<0>(mctpInfo));
        existingMctpInfos.erase(std::remove(existingMctpInfos.begin(),
                                            existingMctpInfos.end(), mctpInfo),
                                existingMctpInfos.end());
    }
}

void MctpDiscovery::propertiesChangedCb(sdbusplus::message_t& msg)
{
    using Interface = std::string;
    using Property = std::string;
    using Value = std::string;
    using Properties = std::map<Property, std::variant<Value>>;

    Interface interface;
    Properties properties;
    std::string objPath{};
    std::string service{};

    try
    {
        msg.read(interface, properties);
        objPath = msg.get_path();
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Error handling Connectivity property changed message, error - {ERROR}",
            "ERROR", e);
        return;
    }

    for (const auto& [key, valueVariant] : properties)
    {
        Value propVal = std::get<std::string>(valueVariant);
        auto availability = (propVal == "Available") ? true : false;

        if (key == MCTPConnectivityProp)
        {
            try
            {
                service = pldm::utils::DBusHandler().getService(
                    objPath.c_str(), MCTPEndpoint::interface);
            }
            catch (const sdbusplus::exception_t& e)
            {
                error(
                    "Error getting service for path '{PATH}', error - {ERROR}",
                    "PATH", objPath, "ERROR", e);
                return;
            }
            const MctpEndpointProps& epProps =
                getMctpEndpointProps(service, objPath);

            auto types = std::get<MCTPMsgTypes>(epProps);
            if (!std::ranges::contains(types, mctpTypePLDM))
            {
                return;
            }
            const UUID& uuid = getEndpointUUIDProp(service, objPath);

            MctpInfo mctpInfo(std::get<eid>(epProps), uuid, "",
                              std::get<NetworkId>(epProps), std::nullopt,
                              std::nullopt);
            searchConfigurationFor(pldm::utils::DBusHandler(), mctpInfo);
            if (!std::ranges::contains(existingMctpInfos, mctpInfo))
            {
                if (availability)
                {
                    // The endpoint not in existingMctpInfos and is
                    // available Add it to existingMctpInfos
                    info(
                        "Adding Endpoint networkId {NETWORK} ID {EID} by propertiesChanged signal",
                        "NETWORK", std::get<3>(mctpInfo), "EID",
                        unsigned(std::get<0>(mctpInfo)));
                    addToExistingMctpInfos(MctpInfos(1, mctpInfo));
                    handleMctpEndpoints(MctpInfos(1, mctpInfo));
                }
            }
            else
            {
                // The endpoint already in existingMctpInfos
                updateMctpEndpointAvailability(mctpInfo, availability);
            }
        }
    }
}

void MctpDiscovery::discoverEndpoints(sdbusplus::message_t& msg)
{
    MctpInfos addedInfos;
    getAddedMctpInfos(msg, addedInfos);
    addToExistingMctpInfos(addedInfos);
    handleMctpEndpoints(addedInfos);
}

void MctpDiscovery::removeEndpoints(sdbusplus::message_t&)
{
    MctpInfos mctpInfos;
    MctpInfos removedInfos;
    std::map<MctpInfo, Availability> currentMctpInfoMap;
    getMctpInfos(currentMctpInfoMap);
    for (const auto& mapIt : currentMctpInfoMap)
    {
        mctpInfos.push_back(mapIt.first);
    }
    removeFromExistingMctpInfos(mctpInfos, removedInfos);
    handleRemovedMctpEndpoints(removedInfos);
    removeConfigs(removedInfos);
}

void MctpDiscovery::handleMctpEndpoints(const MctpInfos& mctpInfos)
{
    for (const auto& handler : handlers)
    {
        if (handler)
        {
            handler->handleConfigurations(configurations);
            handler->handleMctpEndpoints(mctpInfos);
        }
    }
}

void MctpDiscovery::handleRemovedMctpEndpoints(const MctpInfos& mctpInfos)
{
    for (const auto& handler : handlers)
    {
        if (handler)
        {
            handler->handleRemovedMctpEndpoints(mctpInfos);
        }
    }
}

void MctpDiscovery::updateMctpEndpointAvailability(const MctpInfo& mctpInfo,
                                                   Availability availability)
{
    for (const auto& handler : handlers)
    {
        if (handler)
        {
            handler->updateMctpEndpointAvailability(mctpInfo, availability);
        }
    }
}

std::string MctpDiscovery::getNameFromProperties(
    const utils::PropertyMap& properties)
{
    if (!properties.contains("Name"))
    {
        error("Missing name property");
        return "";
    }
    return std::get<std::string>(properties.at("Name"));
}

std::string MctpDiscovery::constructMctpReactorObjectPath(
    const MctpInfo& mctpInfo)
{
    const auto networkId = std::get<NetworkId>(mctpInfo);
    const auto eid = std::get<pldm::eid>(mctpInfo);
    return std::string{MCTPPath} + "/networks/" + std::to_string(networkId) +
           "/endpoints/" + std::to_string(eid) + "/configured_by";
}

bool MctpDiscovery::resolveAssociation(const pldm::utils::DBusHandler& handler,
                                       MctpInfo& mctpInfo)
{
    const auto mctpReactorObjectPath = constructMctpReactorObjectPath(mctpInfo);

    try
    {
        // Define the search scope for the association
        sdbusplus::object_path inventorySubtreePath(inventorySubtreePathStr);
        constexpr auto subTreeDepth = 3;

        // Query the ObjectMapper for associated subtrees
        auto response = handler.getAssociatedSubTree(
            mctpReactorObjectPath, inventorySubtreePath, subTreeDepth,
            interfaceFilter);

        if (response.empty())
        {
            return false;
        }

        // Parse the response (map<Path, map<Service, vector<Interface>>>)
        // We take the first result found.
        auto subTree = response.begin();
        std::string associatedObjPath = subTree->first;
        auto associatedServiceProp = subTree->second;

        if (associatedServiceProp.empty())
        {
            return false;
        }

        auto entry = associatedServiceProp.begin();
        std::string associatedService = entry->first;
        auto dBusIntfList = entry->second;

        // Check if the service implements any of the interfaces in our filter
        auto it = std::find_if(
            dBusIntfList.begin(), dBusIntfList.end(), [](const auto& intf) {
                return std::ranges::contains(interfaceFilter, intf);
            });

        if (it != dBusIntfList.end())
        {
            // Avoid an extra D-Bus call by using the path segment as name.
            auto name = sdbusplus::object_path(associatedObjPath).filename();
            lg2::info("resolveAssociation: EID {EID} path {OBJ} name {NAME}",
                      "EID", std::get<pldm::eid>(mctpInfo), "OBJ",
                      associatedObjPath, "NAME", name);
            if (!name.empty())
            {
                std::get<4>(mctpInfo) = name;
            }
            std::get<5>(mctpInfo) = associatedObjPath;

            // Update the internal configuration map with the resolved info
            configurations.insert_or_assign(associatedObjPath, mctpInfo);
            return true;
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Error resolving association for {PATH}: {ERR}", "PATH",
                   mctpReactorObjectPath, "ERR", e);
    }

    return false;
}

void MctpDiscovery::searchConfigurationFor(
    const pldm::utils::DBusHandler& handler, MctpInfo& mctpInfo)
{
    const auto mctpReactorObjectPath = constructMctpReactorObjectPath(mctpInfo);

    // If resolution failed, check if we are already monitoring this path.
    if (associationMatches.contains(mctpReactorObjectPath))
    {
        return;
    }

    // Create an asynchronous D-Bus Match rule to wait for the association.
    auto networkId = std::get<3>(mctpInfo);
    auto eid = std::get<0>(mctpInfo);

    // Construct the path that we expect to appear in the 'InterfacesAdded'
    // signal
    std::string matchPath =
        std::string(MCTPPath) + "/networks/" + std::to_string(networkId) +
        "/endpoints/" + std::to_string(eid);

    try
    {
        associationMatches[mctpReactorObjectPath] = std::make_unique<
            sdbusplus::bus::match_t>(
            pldm::utils::DBusHandler::getBus(),
            sdbusplus::bus::match::rules::interfacesAdded() +
                sdbusplus::bus::match::rules::argNpath(0, matchPath),
            [this, mctpInfo,
             mctpReactorObjectPath](sdbusplus::message_t& msg) mutable {
                sdbusplus::object_path objPath;
                using InterfaceMap = std::map<
                    std::string,
                    std::map<std::string, std::variant<std::vector<std::string>,
                                                       std::string, bool>>>;
                InterfaceMap interfaces;

                try
                {
                    msg.read(objPath, interfaces);
                }
                catch (...)
                {
                    return;
                }

                if (interfaces.contains(
                        "xyz.openbmc_project.Association.Definitions"))
                {
                    lg2::info(
                        "Deferred Association signal received for {PATH}.",
                        "PATH", mctpReactorObjectPath);
                    std::unique_ptr<sdbusplus::bus::match_t> lifeExtender;
                    auto matchIt =
                        this->associationMatches.find(mctpReactorObjectPath);
                    if (matchIt != this->associationMatches.end())
                    {
                        lifeExtender = std::move(matchIt->second);
                        this->associationMatches.erase(matchIt);
                    }
                    pldm::utils::DBusHandler dhandler;
                    // Attempt to resolve the association now that the signal
                    // arrived
                    if (this->resolveAssociation(dhandler, mctpInfo))
                    {
                        this->addToExistingMctpInfos(MctpInfos(1, mctpInfo));
                        this->handleMctpEndpoints(MctpInfos(1, mctpInfo));
                        this->retryPendingBridgeChildren(
                            dhandler, std::get<pldm::eid>(mctpInfo));
                    }
                }
            });
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to create match rule for {PATH}: {ERR}", "PATH",
                   mctpReactorObjectPath, "ERR", e);
    }

    // Synchronous attempt after registering the match rule. If the association
    // already exists, resolve it immediately and cancel the deferred rule.
    // Registering first ensures that if InterfacesAdded arrives while
    // resolveAssociation's bus.call() blocks the event loop, the signal lands
    // in the socket buffer and is processed when the event loop resumes.
    if (resolveAssociation(handler, mctpInfo))
    {
        associationMatches.erase(mctpReactorObjectPath);
        retryPendingBridgeChildren(handler, std::get<pldm::eid>(mctpInfo));
    }
    else if (matchBridgePoolConfig(handler, mctpInfo))
    {
        associationMatches.erase(mctpReactorObjectPath);
    }
    else
    {
        // Bridge not confirmed yet - retry when another endpoint resolves.
        pendingBridgeChildren.push_back(mctpInfo);
    }
}

namespace
{
// EM's $bus-arithmetic template always produces a JSON string even for numeric
// fields; a literal integer in the config stays a native D-Bus numeric. Accept both.
std::optional<uint64_t> toUint64(const pldm::utils::PropertyValue& val)
{
    return std::visit(
        [](auto&& v) -> std::optional<uint64_t> {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
            {
                try
                {
                    return static_cast<uint64_t>(std::stoull(v));
                }
                catch (const std::exception&)
                {
                    return std::nullopt;
                }
            }
            else if constexpr (std::is_arithmetic_v<T>)
            {
                return static_cast<uint64_t>(v);
            }
            else
            {
                return std::nullopt;
            }
        },
        val);
}
} // namespace

std::optional<std::pair<eid, eid>> MctpDiscovery::getBridgePoolRange(
    const pldm::utils::DBusHandler& handler, NetworkId networkId,
    eid bridgeEid)
{
    auto cacheIt = bridgePoolCache.find(bridgeEid);
    if (cacheIt != bridgePoolCache.end())
    {
        return cacheIt->second;
    }

    constexpr auto bridgeIntf = "au.com.codeconstruct.MCTP.Bridge1";
    std::string endpointObjPath = std::string(MCTPPath) + "/networks/" +
                                  std::to_string(networkId) + "/endpoints/" +
                                  std::to_string(bridgeEid);

    try
    {
        auto properties = handler.getDbusPropertiesVariant(
            MCTPService, endpointObjPath.c_str(), bridgeIntf);

        auto startIt = properties.find("PoolStart");
        auto endIt = properties.find("PoolEnd");
        auto start = startIt == properties.end() ? std::nullopt
                                                  : toUint64(startIt->second);
        auto end = endIt == properties.end() ? std::nullopt
                                             : toUint64(endIt->second);
        if (!start || !end)
        {
            return std::nullopt;
        }

        auto range = std::make_pair(static_cast<eid>(*start),
                                    static_cast<eid>(*end));
        bridgePoolCache[bridgeEid] = range;
        return range;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

bool MctpDiscovery::matchBridgePoolConfig(
    const pldm::utils::DBusHandler& handler, MctpInfo& mctpInfo)
{
    constexpr auto staticEndpointIntf =
        "xyz.openbmc_project.Configuration.MCTPBridgeChild";
    const auto childEid = std::get<eid>(mctpInfo);
    const auto networkId = std::get<NetworkId>(mctpInfo);

    for (const auto& [bridgeObjPath, bridgeInfo] : configurations)
    {
        const auto bridgeEid = std::get<eid>(bridgeInfo);
        if (bridgeEid == childEid ||
            std::get<NetworkId>(bridgeInfo) != networkId)
        {
            continue;
        }

        auto range = getBridgePoolRange(handler, networkId, bridgeEid);
        if (!range || childEid < range->first || childEid > range->second)
        {
            continue;
        }

        const uint64_t offset = childEid - range->first;

        // EM configs for pool members are siblings of the bridge record,
        // not children - scope the search to the bridge's parent path.
        auto lastSlash = bridgeObjPath.find_last_of('/');
        if (lastSlash == std::string::npos)
        {
            continue;
        }
        std::string boardPath = bridgeObjPath.substr(0, lastSlash);

        pldm::utils::GetSubTreeResponse response;
        try
        {
            response = handler.getSubtree(boardPath, 0, {staticEndpointIntf});
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Failed to query bridge-child configs under {PATH}, error - {ERR}",
                "PATH", boardPath, "ERR", e);
            continue;
        }

        for (const auto& [objPath, serviceMap] : response)
        {
            for (const auto& serviceEntry : serviceMap)
            {
                const auto& service = serviceEntry.first;
                try
                {
                    auto properties = handler.getDbusPropertiesVariant(
                        service.c_str(), objPath.c_str(), staticEndpointIntf);

                    auto offsetIt = properties.find("PoolOffset");
                    auto offsetParsed = offsetIt == properties.end()
                                             ? std::nullopt
                                             : toUint64(offsetIt->second);
                    if (!offsetParsed || *offsetParsed != offset)
                    {
                        continue;
                    }

                    auto nameIt = properties.find("Name");
                    if (nameIt != properties.end() &&
                        std::holds_alternative<std::string>(nameIt->second))
                    {
                        std::get<4>(mctpInfo) =
                            std::get<std::string>(nameIt->second);
                    }
                    std::get<5>(mctpInfo) = objPath;

                    configurations.insert_or_assign(objPath, mctpInfo);
                    lg2::info(
                        "Matched bridge-child config {PATH} for EID {EID} via bridge {BRIDGE} at offset {OFFSET}",
                        "PATH", objPath, "EID", childEid, "BRIDGE", bridgeEid,
                        "OFFSET", offset);
                    return true;
                }
                catch (const std::exception& e)
                {
                    lg2::error(
                        "Error reading bridge-child config at {PATH}, error - {ERR}",
                        "PATH", objPath, "ERR", e);
                }
            }
        }
    }
    return false;
}

void MctpDiscovery::retryPendingBridgeChildren(
    const pldm::utils::DBusHandler& handler, eid bridgeEid)
{
    auto configIt = std::find_if(
        configurations.begin(), configurations.end(),
        [bridgeEid](const auto& entry) {
            return std::get<eid>(entry.second) == bridgeEid;
        });
    if (configIt == configurations.end())
    {
        return;
    }

    auto networkId = std::get<NetworkId>(configIt->second);
    auto range = getBridgePoolRange(handler, networkId, bridgeEid);
    if (!range)
    {
        return;
    }

    // mctp-reactor only assigns the bridge's own EID; enumerate the pool here.
    learnBridgePoolMembers(handler, networkId, bridgeEid, configIt->first,
                           range->first, range->second);

    if (pendingBridgeChildren.empty())
    {
        return;
    }

    auto pending = std::move(pendingBridgeChildren);
    pendingBridgeChildren.clear();

    for (auto mctpInfo : pending)
    {
        if (matchBridgePoolConfig(handler, mctpInfo))
        {
            addToExistingMctpInfos(MctpInfos(1, mctpInfo));
            handleMctpEndpoints(MctpInfos(1, mctpInfo));
        }
        else
        {
            pendingBridgeChildren.push_back(mctpInfo);
        }
    }
}

std::set<uint64_t> MctpDiscovery::getConfiguredPoolOffsets(
    const pldm::utils::DBusHandler& handler, const std::string& bridgeObjPath)
{
    constexpr auto staticEndpointIntf =
        "xyz.openbmc_project.Configuration.MCTPBridgeChild";
    std::set<uint64_t> offsets;

    auto lastSlash = bridgeObjPath.find_last_of('/');
    if (lastSlash == std::string::npos)
    {
        return offsets;
    }
    std::string boardPath = bridgeObjPath.substr(0, lastSlash);

    pldm::utils::GetSubTreeResponse response;
    try
    {
        response = handler.getSubtree(boardPath, 0, {staticEndpointIntf});
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to query bridge-child configs under {PATH}, error - {ERR}",
            "PATH", boardPath, "ERR", e);
        return offsets;
    }

    for (const auto& [objPath, serviceMap] : response)
    {
        for (const auto& serviceEntry : serviceMap)
        {
            const auto& service = serviceEntry.first;
            try
            {
                auto properties = handler.getDbusPropertiesVariant(
                    service.c_str(), objPath.c_str(), staticEndpointIntf);
                auto offsetIt = properties.find("PoolOffset");
                auto offsetParsed = offsetIt == properties.end()
                                         ? std::nullopt
                                         : toUint64(offsetIt->second);
                if (offsetParsed)
                {
                    offsets.insert(*offsetParsed);
                }
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Error reading bridge-child config at {PATH}, error - {ERR}",
                    "PATH", objPath, "ERR", e);
            }
        }
    }
    return offsets;
}

void MctpDiscovery::learnBridgePoolMembers(
    const pldm::utils::DBusHandler& handler, NetworkId networkId,
    eid bridgeEid, const std::string& bridgeObjPath, eid poolStart,
    eid poolEnd)
{
    constexpr auto networkIntf = "au.com.codeconstruct.MCTP.Network1";
    std::string networkObjPath =
        std::string(MCTPPath) + "/networks/" + std::to_string(networkId);

    // Only attempt offsets with an EM config record - unwired pool slots
    // would fail every retry indefinitely.
    auto offsets = getConfiguredPoolOffsets(handler, bridgeObjPath);
    if (offsets.empty())
    {
        return;
    }

    std::set<uint64_t> failedOffsets;
    for (const auto offset : offsets)
    {
        eid candidate = poolStart + static_cast<eid>(offset);
        if (candidate > poolEnd)
        {
            continue;
        }
        try
        {
            auto& bus = pldm::utils::DBusHandler::getBus();
            auto method = bus.new_method_call(MCTPService,
                                              networkObjPath.c_str(),
                                              networkIntf, "LearnEndpoint");
            method.append(candidate);
            bus.call(method);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Failed to LearnEndpoint {EID} (offset {OFFSET}) on network {NET}, error - {ERR}",
                "EID", candidate, "OFFSET", offset, "NET", networkId, "ERR",
                e);
            failedOffsets.insert(offset);
        }
    }

    if (failedOffsets.empty())
    {
        pendingBridgeLearns.erase(bridgeEid);
        return;
    }

    // Device may still be powering on - retry rather than treating first
    // failure as permanent.
    auto& pending = pendingBridgeLearns[bridgeEid];
    pending.networkId = networkId;
    pending.poolStart = poolStart;
    pending.pendingOffsets = std::move(failedOffsets);
    pending.attempts = 0;

    if (!bridgeLearnTimer.isEnabled())
    {
        bridgeLearnTimer.restart(bridgeLearnRetryInterval);
    }
}

void MctpDiscovery::retryBridgeLearn()
{
    constexpr auto networkIntf = "au.com.codeconstruct.MCTP.Network1";

    for (auto it = pendingBridgeLearns.begin();
         it != pendingBridgeLearns.end();)
    {
        auto& pending = it->second;
        pending.attempts++;

        std::string networkObjPath = std::string(MCTPPath) + "/networks/" +
                                     std::to_string(pending.networkId);

        std::set<uint64_t> stillFailed;
        for (const auto offset : pending.pendingOffsets)
        {
            eid candidate = pending.poolStart + static_cast<eid>(offset);
            try
            {
                auto& bus = pldm::utils::DBusHandler::getBus();
                auto method = bus.new_method_call(MCTPService,
                                                  networkObjPath.c_str(),
                                                  networkIntf,
                                                  "LearnEndpoint");
                method.append(candidate);
                bus.call(method);
            }
            catch (const std::exception&)
            {
                stillFailed.insert(offset);
            }
        }

        if (stillFailed.empty())
        {
            it = pendingBridgeLearns.erase(it);
            continue;
        }

        if (pending.attempts >= bridgeLearnMaxAttempts)
        {
            lg2::error(
                "Giving up on LearnEndpoint for bridge {EID}, network {NET} - {COUNT} pool offset(s) never responded after {ATTEMPTS} attempts",
                "EID", it->first, "NET", pending.networkId, "COUNT",
                stillFailed.size(), "ATTEMPTS", pending.attempts);
            it = pendingBridgeLearns.erase(it);
            continue;
        }

        pending.pendingOffsets = std::move(stillFailed);
        ++it;
    }

    if (pendingBridgeLearns.empty())
    {
        bridgeLearnTimer.setEnabled(false);
    }
    else
    {
        bridgeLearnTimer.restart(bridgeLearnRetryInterval);
    }
}

void MctpDiscovery::removeConfigs(const MctpInfos& removedInfos)
{
    for (const auto& mctpInfo : removedInfos)
    {
        const auto eidToRemove = std::get<eid>(mctpInfo);
        const auto netToRemove = std::get<NetworkId>(mctpInfo);

        std::erase_if(configurations, [eidToRemove,
                                       netToRemove](const auto& config) {
            const auto& [__, mctpInfo] = config;
            const auto eidValue = std::get<eid>(mctpInfo);
            const auto netValue = std::get<NetworkId>(mctpInfo);

            return eidValue == eidToRemove && netValue == netToRemove;
        });

        std::erase_if(pendingBridgeChildren,
                      [eidToRemove, netToRemove](const auto& mctpInfo) {
                          return std::get<eid>(mctpInfo) == eidToRemove &&
                                 std::get<NetworkId>(mctpInfo) == netToRemove;
                      });
        bridgePoolCache.erase(eidToRemove);
    }
}

} // namespace pldm
