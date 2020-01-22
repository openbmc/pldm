#include "file_io_type_dump.hpp"

#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <exception>
#include <filesystem>
#include <iostream>

#include "libpldm/base.h"
#include "oem/ibm/libpldm/file_io.h"

namespace pldm
{
namespace responder
{

int DumpHandler::fd = -1;

int DumpHandler::processNewFileNotification(uint32_t length)
{
    std::cout << "length of dump is " << length
              << std::endl; // to get rid of unused
#if 0
    static constexpr auto dumpObjPath = "";
    static constexpr auto dumpInterface = "";

    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    Variant value;
    try
    {
        auto service = getService(bus,dumpObjPath,dumpInterface);
        //using namespace sdbusplus::  ;
        //need to add the dump id and size as method parameter
        auto method = bus.new_method_call(service.c_str(),dumpObjPath,
                                   dumpInterface,"Create"); //change the method name accordingly
        //method.append();
        auto reply = bus.call(method);                           
        reply.read(value);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to DUMP manager, ERROR="
            << e.what() << "\n";
        return PLDM_ERROR;    
    }

#endif
    return PLDM_SUCCESS; // or return value?
}

int DumpHandler::writeFromMemory(uint32_t offset, uint32_t length,
                                 uint64_t address)
{
    static constexpr auto nbdInterface = "/dev/nbd1";
    int flags = O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE;

    if (DumpHandler::fd == -1)
    {
        DumpHandler::fd = open(nbdInterface, flags);
        if (DumpHandler::fd == -1)
        {
            std::cerr << "NBD file does not exist at " << nbdInterface << "\n";
            return PLDM_ERROR;
        }
    }
    return transferFileData(DumpHandler::fd, false, offset, length, address);
}

int DumpHandler::fileAck(uint8_t /*fileStatus*/)
{
    if (fd <= 0)
    {
        // std::cerr << "File does not exist. Invalid FD.";
        close(fd);
        fd = -1;
        return PLDM_ERROR;
    }
    else if (fd > 0)
    {
        static constexpr auto dumpObjPath = "/xyz/openbmc_project/Dump/Entry";
        static constexpr auto dumpInterface =
            "org.openbmc_project.Dump.Entry.System.interace";
        static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
        std::variant<uint32_t> value;
        try
        {

            auto service =
                pldm::utils::getService(bus, dumpObjPath, dumpInterface);

            auto method = bus.new_method_call(service.c_str(), dumpObjPath,
                                              dbusProperties, "Get");
            method.append(dumpInterface, "SourceDumpId");

            auto reply = bus.call(method);
            reply.read(value);
        }
        catch (const std::exception& e)
        {
            return PLDM_ERROR;
        }

        auto dbusInfo = handle.inventoryLookup();

        // Read the all the inventory D-Bus objects
        auto& bus = pldm::utils::DBusHandler::getBus();
        dbus::ObjectValueTree objects;

        try
        {
            auto method = bus.new_method_call(
                std::get<0>(dbusInfo).c_str(), std::get<1>(dbusInfo).c_str(),
                "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
            auto reply = bus.call(method);
            reply.read(objects);
        }
        catch (const std::exception& e)
        {
            return;
        }

        // Populate all the interested Item types to a map for easy lookup
        std::set<dbus::Interface> itemIntfsLookup;
        auto itemIntfs = std::get<2>(dbusInfo);
        std::transform(std::begin(itemIntfs), std::end(itemIntfs),
                       std::inserter(itemIntfsLookup, itemIntfsLookup.end()),
                       [](dbus::Interface intf) { return intf; });
        static constexpr auto dumpObjPath2 = "/xyz/openbmc_project/Dump/Entry";
        static constexpr auto dumpInterface2 =
            "org.openbmc_project.Dump.Entry.interace";

        for (const auto& object : objects)
        {
            if (itemIntfsLookup.find(value) == fd)
            {
                try
                {
                    auto service = pldm::utils::getService(bus, dumpObjPath2,
                                                           dumpInterface2);

                    auto method = bus.new_method_call(
                        service.c_str(), dumpObjPath2, dbusProperties, "Set");
                    method.append(dumpInterface2, "Offloaded");
                    bus.call_noreply(method);
                }
                catch (const std::exception& e)
                {
                    return;
                }
            }
        }
        close(fd);
        fd = -1;
    }

#endif
    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
