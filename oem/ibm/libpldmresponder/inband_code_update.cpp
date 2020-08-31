#include "inband_code_update.hpp"

namespace pldm
{

namespace responder
{

std::string CodeUpdate::fetchCurrentBootSide()
{
    return currBootSide;
}

std::string CodeUpdate::fetchNextBootSide()
{
    return nextBootSide;
}

int CodeUpdate::setCurrentBootSide(std::string currSide)
{
    currBootSide = currSide;
    return PLDM_SUCCESS;
}

int CodeUpdate::setNextBootSide(std::string nextSide)
{
    nextBootSide = nextSide;
    return PLDM_SUCCESS;
}

} // namespace responder

} // namespace pldm
