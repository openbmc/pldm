#include "config.h"

#include "softoff.hpp"

#include <iostream>

int main()
{
    pldm::SoftPowerOff softPower;

    if (softPower.isHasError() == true)
    {
        std::cerr << "Host failed to gracefully shutdown,exit the "
                     "pldm-softpoweroff\n";
        return -1;
    }

    return 0;
}
