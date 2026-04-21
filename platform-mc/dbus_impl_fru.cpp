#include "dbus_impl_fru.hpp"

namespace pldm
{
namespace dbus_api
{

std::string PldmFruDecorators::partNumber(std::string value)
{
    return assetserver::partNumber(value);
}

std::string PldmFruDecorators::serialNumber(std::string value)
{
    return assetserver::serialNumber(value);
}

std::string PldmFruDecorators::manufacturer(std::string value)
{
    return assetserver::manufacturer(value);
}

std::string PldmFruDecorators::buildDate(std::string value)
{
    return assetserver::buildDate(value);
}

std::string PldmFruDecorators::model(std::string value)
{
    return assetserver::model(value);
}

std::string PldmFruDecorators::subModel(std::string value)
{
    return assetserver::subModel(value);
}

std::string PldmFruDecorators::sparePartNumber(std::string value)
{
    return assetserver::sparePartNumber(value);
}

std::string PldmFruDecorators::assetTag(std::string value)
{
    return assettagserver::assetTag(value);
}

std::string PldmFruDecorators::version(std::string value)
{
    return revisionserver::version(value);
}

std::vector<std::string> PldmFruDecorators::names(
    std::vector<std::string> values)
{
    return compatibleserver::names(values);
}

} // namespace dbus_api
} // namespace pldm
