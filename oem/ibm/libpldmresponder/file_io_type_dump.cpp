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
#if 0
    static constexpr auto dumpObjPath = "/xyz/openbmc_project/Dump";
    static constexpr auto dumpInterface = "org.openbmc_project.Dump.Entry.Status";
    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    if (fd <= 0)
    {
        std::cerr << "File does not exist. Invalid FD.";
        close(fd);
        fd = -1;
        return PLDM_ERROR;
    }
    else if (fd > 0)
    {    
        try
        {
            auto service = getService(bus, dumpObjPath, dumpInterface);
            auto method = bus.new_method_call(service.c_str(), dumpObjPath,
                                          dumpInterface, "OffloadComplete");
            method.append(fileHandle);
            bus.call_noreply(method);
        }
        catch (const std::exception& e)
        {
     I      std::cerr << "Dump Offload Ack D-Bus call failed";
            return PLDM_ERROR;
        }
        close(fd);
        fd = -1;
    }

#endif
    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm
