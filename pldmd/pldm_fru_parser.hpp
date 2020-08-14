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
    PLDMFRUParser(sdbusplus::bus::bus& bus, std::string objPath,
                  DBusHandler* const dbusHandler) :
        T(bus, objPath.c_str()),
        dbusHandler(dbusHandler)
    {}

    /**
     */
    int parseFRUTable(const uint8_t* fruTable, size_t fruTableLength,
                      size_t recordStartOffset, DBusHandler* const dbusHandler);

    std::vector<PLDMFRUParser*> getFRUParserObjs()
    {
        return fruParserObjs;
    }

    /** @brief dbus handler */
    DBusHandler* const dbusHandler;

  private:
    /** @brief vector of PLDM FRU Parser objects */
    std::vector<PLDMFRUParser*> fruParserObjs;
};

} // namespace fruparser
} // namespace pldm
