#include "libpldm/base.h"

#include "softoff.hpp"

#include <iostream>

int main()
{
    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    pldm::SoftPowerOff softPower;

    if (softPower.isError() == true)
    {
        std::cerr << "Host failed to gracefully shutdown, exiting "
                     "pldm-softpoweroff app\n";
        return -1;
    }

    // Send the gracefully shutdown request to the host and
    // wait the host gracefully shutdown.
    auto rc = softPower.hostSoftOff(event);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "pldm-softpoweroff:Failure in sending soft off request to "
                     "the host. Exiting pldm-softpoweroff app\n";

        return -1;
    }

    return 0;
}
