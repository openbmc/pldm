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

} // namespace responder
} // namespace pldm
