#include "common/utils.hpp"
#include "softoff.hpp"

#include <getopt.h>

#include <iostream>

int main(int argc, char* argv[])
{

    bool noTimeOut = false;
    static struct option long_options[] = {{"notimeout", no_argument, 0, 't'},
                                           {0, 0, 0, 0}};

    auto argflag = getopt_long(argc, argv, "t", long_options, nullptr);
    switch (argflag)
    {
        case 't':
            noTimeOut = true;
            std::cout << "Not applying any time outs\n";
            break;
        case -1:
            break;
        default:
            exit(EXIT_FAILURE);
    }

    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    // Get a handle to system D-Bus.
    auto& bus = pldm::utils::DBusHandler::getBus();

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

    pldm::SoftPowerOff softPower(bus, event.get(), noTimeOut);

    if (softPower.isError())
    {
        std::cerr << "Host failed to gracefully shutdown, exiting "
                     "pldm-softpoweroff app\n";
        return -1;
    }

    if (softPower.isCompleted())
    {
        std::cerr << "Host current state is not Running, exiting "
                     "pldm-softpoweroff app\n";
        return 0;
    }

    // Send the gracefully shutdown request to the host and
    // wait the host gracefully shutdown.
    if (softPower.hostSoftOff(event))
    {
        std::cerr << "pldm-softpoweroff:Failure in sending soft off request to "
                     "the host. Exiting pldm-softpoweroff app\n";

        return -1;
    }

    if (softPower.isTimerExpired() && softPower.isReceiveResponse())
    {
        pldm::utils::reportError(
            "pldm soft off: Waiting for the host soft off timeout");
        std::cerr
            << "PLDM host soft off: ERROR! Wait for the host soft off timeout."
            << "Exit the pldm-softpoweroff "
            << "\n";
        return -1;
    }

    return 0;
}
