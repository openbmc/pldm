#include "fru.h"

#include "common/utils.hpp"
#include "xyz/openbmc_project/Inventory/Source/PLDM/FRU/server.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>

#include <string>

using namespace pldm::utils;

namespace pldm
{

namespace fruparser
{
std::string interface = "xyz.openbmc_project.Inventory.Source.PLDM.FRU";

using FruTableIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Source::PLDM::server::FRU>;

template <typename T>
class PLDMFRUParser : public FruTableIntf
{

  public:
    PLDMFRUParser() = delete;
    PLDMFRUParser(const PLDMFRUParser&) = delete;
    PLDMFRUParser& operator=(const PLDMFRUParser&) = delete;
    PLDMFRUParser(PLDMFRUParser&&) = delete;
    PLDMFRUParser& operator=(PLDMFRUParser&&) = delete;
    virtual ~PLDMFRUParser() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     *  @param[in] dbusHandler - system bus handle
     **/
    PLDMFRUParser(sdbusplus::bus::bus& bus, std::string objectPath) :
        T(bus, objectPath.c_str())
    {
        dBusMap.objectPath = objectPath;
        dBusMap.interface = interface;
        dBusMap.propertyName = "property_name";
        dBusMap.propertyType = "property_type";
    }

    /**
     */
    int parseFRUTable(const uint8_t* fruTable, size_t fruTableLength,
                      size_t recordStartOffset, DBusHandler* const dbusHandler);

    std::vector<PLDMFRUParser*> getFRUParserObjs()
    {
        return fruParserObjs;
    }

  private:
    /** @brief vector of PLDM FRU Parser objects */
    std::vector<PLDMFRUParser*> fruParserObjs;

    /** @brief dbus object */
    DBusMapping dBusMap;
};

} // namespace fruparser
} // namespace pldm
