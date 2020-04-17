#include "config.h"

#include "softoff.hpp"

#include <iostream>

int main()
{
    pldm::SoftPowerOff softPower;

    if (softPower.isError() == true)
    {
        std::cerr << "Host failed to gracefully shutdown, exiting "
                     "pldm-softpoweroff app\n";
        return -1;
    }

    if (softPower.isCompleted() == true)
    {
        return 0;
    }

    return 0;
}
