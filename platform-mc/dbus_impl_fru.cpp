#include "dbus_impl_fru.hpp"

#include <iostream>

namespace pldm
{
namespace dbus_api
{

std::string PldmEntityReq::partNumber(std::string value)
{
    return assetserver::partNumber(value);
}

std::string PldmEntityReq::serialNumber(std::string value)
{
    return assetserver::serialNumber(value);
}

std::string PldmEntityReq::manufacturer(std::string value)
{
    return assetserver::manufacturer(value);
}

std::string PldmEntityReq::buildDate(std::string value)
{
    return assetserver::buildDate(value);
}

std::string PldmEntityReq::model(std::string value)
{
    return assetserver::model(value);
}

std::string PldmEntityReq::subModel(std::string value)
{
    return assetserver::subModel(value);
}

std::string PldmEntityReq::sparePartNumber(std::string value)
{
    return assetserver::sparePartNumber(value);
}

std::string PldmEntityReq::assetTag(std::string value)
{
    return assettagserver::assetTag(value);
}

std::string PldmEntityReq::version(std::string value)
{
    return revisionserver::version(value);
}

std::vector<std::string> PldmEntityReq::names(std::vector<std::string> values)
{
    return compatibleserver::names(values);
}

} // namespace dbus_api
} // namespace pldm
