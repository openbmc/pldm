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
     **/
    PLDMFRUParser(sdbusplus::bus::bus& bus, std::string objectPath) :
        T(bus, objectPath.c_str()), objectPath(objectPath)
    {
        dBusMap.objectPath = objectPath;
        dBusMap.interface = interface;
    }

    /** @brief Method to parser fru table properties and add to dbus.
     *  @param[in] fru_table - Pointer to the FRU table
     *  @param[in] record_start_offset - Offset from where the record data
     *starts in fru_table
     *  @param[in] fru_table_tength - Total length of the FRU table
     *  @param[in] dBusHandler - dbus handler.
     *  @return pldm_completion_codes
     **/
    int parseFRUTable(const uint8_t* fruTable, size_t fruTableLength,
                      size_t recordStartOffset, DBusHandler* const dbusHandler);

    /** @brief vector of objects created */
    std::vector<std::unique_ptr<PLDMFRUParser<T>>> pldmfruObjs;

    /** @brief dbus object */
    DBusMapping dBusMap;

    /** @brief dbus object path */
    std::string objectPath;
};

} // namespace fruparser
} // namespace pldm
