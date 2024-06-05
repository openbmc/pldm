#include "dbus_impl_fru.hpp"

#include <iostream>

namespace pldm
{
namespace dbus_api
{

std::string FruReq::chassisType(std::string value)
{
    return fruserver::chassisType(value);
}

std::string FruReq::model(std::string value)
{
    return fruserver::model(value);
}

std::string FruReq::pn(std::string value)
{
    return fruserver::pn(value);
}

std::string FruReq::sn(std::string value)
{
    return fruserver::sn(value);
}

std::string FruReq::manufacturer(std::string value)
{
    return fruserver::manufacturer(value);
}

std::string FruReq::manufacturerDate(std::string value)
{
    return fruserver::manufacturerDate(value);
}

std::string FruReq::vendor(std::string value)
{
    return fruserver::vendor(value);
}

std::string FruReq::name(std::string value)
{
    return fruserver::name(value);
}

std::string FruReq::sku(std::string value)
{
    return fruserver::sku(value);
}

std::string FruReq::version(std::string value)
{
    return fruserver::version(value);
}

std::string FruReq::assetTag(std::string value)
{
    return fruserver::assetTag(value);
}

std::string FruReq::description(std::string value)
{
    return fruserver::description(value);
}

std::string FruReq::ecLevel(std::string value)
{
    return fruserver::ecLevel(value);
}

std::string FruReq::other(std::string value)
{
    return fruserver::other(value);
}

uint32_t FruReq::iana(uint32_t value)
{
    return fruserver::iana(value);
}

} // namespace dbus_api
} // namespace pldm
