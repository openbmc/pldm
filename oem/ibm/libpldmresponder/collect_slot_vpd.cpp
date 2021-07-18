#include "collect_slot_vpd.hpp"

#include "libpldm/entity.h"

#include "libpldmresponder/pdr.hpp"
#include "oem_ibm_handler.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <arpa/inet.h>

#include <sdbusplus/server.hpp>

#include <exception>
#include <fstream>
namespace pldm
{
namespace responder
{
using namespace oem_ibm_platform;
using namespace sdbusplus::bus::match::rules;

void SlotEnable::createPresenceMatch(std::string ObjectPath,
                                     StateSetId /*stateSetId*/)
{
    std::cerr << "Create presence match for present property under"
              << ObjectPath << std::endl;
    fruPresenceMatch = std::make_unique<sdbusplus::bus::match::match>(
        pldm::utils::DBusHandler::getBus(),
        propertiesChanged(ObjectPath, "xyz.openbmc_project.Inventory.Item"),
        [this](sdbusplus::message::message& msg) {
            DbusChangedProps props{};
            std::string intf;
            msg.read(intf, props);
            const auto itr = props.find("Present");
            if (itr != props.end())
            {
                PropertyValue value = itr->second;
                auto propVal = std::get<bool>(value);
                if (propVal == true)
                {
                    // set the slot sensor
                    // send a sensor event to host
                }
            }
        });
}

} // namespace responder
} // namespace pldm
