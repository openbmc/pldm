#include "custom_dbus.hpp"

#include "libpldm/state_set.h"

namespace pldm
{
namespace dbus
{

void CustomDBus::setLocationCode(const std::string& path, std::string value)
{
    if (location.find(path) == location.end())
    {
        location.emplace(path,
                         std::make_unique<LocationIntf>(
                             pldm::utils::DBusHandler::getBus(), path.c_str()));
    }

    location.at(path)->locationCode(value);
}

std::string CustomDBus::getLocationCode(const std::string& path) const
{
    if (location.find(path) != location.end())
    {
        return location.at(path)->locationCode();
    }

    return {};
}

void CustomDBus::setOperationalStatus(const std::string& path, bool status)
{
    if (operationalStatus.find(path) == operationalStatus.end())
    {
        operationalStatus.emplace(
            path, std::make_unique<OperationalStatusIntf>(
                      pldm::utils::DBusHandler::getBus(), path.c_str()));
    }

    operationalStatus.at(path)->functional(status);
}

bool CustomDBus::getOperationalStatus(const std::string& path) const
{
    if (operationalStatus.find(path) != operationalStatus.end())
    {
        return operationalStatus.at(path)->functional();
    }

    return false;
}

void CustomDBus::updateItemPresentStatus(const std::string& path)
{
    if (presentStatus.find(path) == presentStatus.end())
    {
        presentStatus.emplace(
            path, std::make_unique<ItemIntf>(pldm::utils::DBusHandler::getBus(),
                                             path.c_str()));
    }

    std::filesystem::path ObjectPath(path);

    // Hardcode the present dbus property to true
    presentStatus.at(path)->present(true);

    // Set the pretty name dbus property to the filename
    // form the dbus path object
    presentStatus.at(path)->prettyName(ObjectPath.filename());
}

void CustomDBus::implementCpuCoreInterface(const std::string& path)
{
    if (cpuCore.find(path) == cpuCore.end())
    {
        cpuCore.emplace(
            path, std::make_unique<CoreIntf>(pldm::utils::DBusHandler::getBus(),
                                             path.c_str()));
        implementObjectEnableIface(path);
    }
}

void CustomDBus::implementObjectEnableIface(const std::string& path)
{
    if (_enabledStatus.find(path) == _enabledStatus.end())
    {
        _enabledStatus.emplace(
            path, std::make_unique<EnableIface>(
                      pldm::utils::DBusHandler::getBus(), path.c_str()));
        _enabledStatus.at(path)->enabled(false);
    }
}

void CustomDBus::setAsserted(
    const std::string& path, const pldm_entity& entity, bool value,
    pldm::host_effecters::HostEffecterParser* hostEffecterParser,
    uint8_t mctpEid, bool isTriggerStateEffecterStates)
{
    if (ledGroup.find(path) == ledGroup.end())
    {
        ledGroup.emplace(
            path, std::make_unique<Group>(pldm::utils::DBusHandler::getBus(),
                                          path.c_str(), hostEffecterParser,
                                          entity, mctpEid));
    }

    ledGroup.at(path)->setStateEffecterStatesFlag(isTriggerStateEffecterStates);

    ledGroup.at(path)->asserted(value);
}

bool CustomDBus::getAsserted(const std::string& path) const
{
    if (ledGroup.find(path) != ledGroup.end())
    {
        return ledGroup.at(path)->asserted();
    }

    return false;
}

bool Group::asserted() const
{
    return sdbusplus::xyz::openbmc_project::Led::server::Group::asserted();
}

bool Group::updateAsserted(bool value)
{
    return sdbusplus::xyz::openbmc_project::Led::server::Group::asserted(value);
}

bool Group::asserted(bool value)
{
    std::vector<set_effecter_state_field> stateField;

    if (value ==
        sdbusplus::xyz::openbmc_project::Led::server::Group::asserted())
    {
        stateField.push_back({PLDM_NO_CHANGE, 0});
    }
    else
    {
        uint8_t state = value ? PLDM_STATE_SET_IDENTIFY_STATE_ASSERTED
                              : PLDM_STATE_SET_IDENTIFY_STATE_UNASSERTED;

        stateField.push_back({PLDM_REQUEST_SET, state});
    }

    if (isTriggerStateEffecterStates)
    {
        if (hostEffecterParser)
        {
            uint16_t effecterId = pldm::utils::findStateEffecterId(
                hostEffecterParser->getPldmPDR(), entity.entity_type,
                entity.entity_instance_num, entity.entity_container_id,
                PLDM_STATE_SET_IDENTIFY_STATE, false);

            hostEffecterParser->sendSetStateEffecterStates(
                mctpEid, effecterId, 1, stateField,
                std::bind(std::mem_fn(&pldm::dbus::Group::updateAsserted), this,
                          std::placeholders::_1),
                value);
            isTriggerStateEffecterStates = true;
            return value;
        }
    }

    isTriggerStateEffecterStates = true;
    return sdbusplus::xyz::openbmc_project::Led::server::Group::asserted(value);
}

const std::vector<std::tuple<std::string, std::string, std::string>>
    CustomDBus::getAssociations(const std::string& path)
{
    if (associations.find(path) != associations.end())
    {
        return associations.at(path)->associations();
    }
    return {};
}

void CustomDBus::setAssociations(const std::string& path, Associations assoc)
{
    using PropVariant = sdbusplus::xyz::openbmc_project::Association::server::
        Definitions::PropertiesVariant;

    if (associations.find(path) == associations.end())
    {
        PropVariant value{std::move(assoc)};
        std::map<std::string, PropVariant> properties;
        properties.emplace("Associations", std::move(value));

        associations.emplace(path, std::make_unique<AssociationsIntf>(
                                       pldm::utils::DBusHandler::getBus(),
                                       path.c_str(), properties));
    }
    else
    {
        // object already created , so just update the associations
        auto currentAssociations = getAssociations(path);
        std::vector<std::tuple<std::string, std::string, std::string>>
            newAssociations;
        newAssociations.reserve(currentAssociations.size() + assoc.size());
        newAssociations.insert(newAssociations.end(),
                               currentAssociations.begin(),
                               currentAssociations.end());
        newAssociations.insert(newAssociations.end(), assoc.begin(),
                               assoc.end());
        associations.at(path)->associations(newAssociations);
    }
}

} // namespace dbus
} // namespace pldm
