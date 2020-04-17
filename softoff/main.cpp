#include "config.h"

#include "softoff.hpp"

#include "libpldm/base.h"

int main()
{
    pldm::PldmSoftPowerOff softPower = pldm::PldmSoftPowerOff();

    if (softPower.isHasError() == true)
    {
        std::cerr << "Exit the pldm-softpoweroff\n";
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}
