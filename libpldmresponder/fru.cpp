#include "fru.hpp"

#include "utils.hpp"

#include <systemd/sd-journal.h>

#include <boost/crc.hpp>
#include <cstring>
#include <iostream>
#include <sdbusplus/bus.hpp>

namespace pldm
{

namespace responder
{

FruImpl::FruImpl(const std::string& configPath)
{
    fru_parser::FruParser handle(configPath);

    auto dbusInfo = handle.inventoryLookup();

    // Read the all the inventory D-Bus objects
    auto bus = sdbusplus::bus::new_default();
    auto method = bus.new_method_call(
        std::get<0>(dbusInfo).c_str(), std::get<1>(dbusInfo).c_str(),
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    auto reply = bus.call(method);

    dbus::ObjectValueTree objects;
    reply.read(objects);

    // Populate all the interested Item types to a map for easy lookup
    std::map<dbus::Interface, uint8_t> itemIntfsMap;
    auto itemIntfs = std::get<2>(dbusInfo);
    std::transform(
        std::begin(itemIntfs), std::end(itemIntfs),
        std::inserter(itemIntfsMap, itemIntfsMap.end()),
        [](dbus::Interface intf) { return std::make_pair(intf, 0); });

    const std::string itemTemplate{"xyz.openbmc_project.Inventory.Item"};

    for (const auto& object : objects)
    {
        const auto& interfaces = object.second;

        for (const auto& interface : interfaces)
        {
            if (interface.first.find(itemTemplate) != std::string::npos)
            {
                if (itemIntfsMap.find(interface.first) != itemIntfsMap.end())
                {
                    // An exception will be thrown by getRecordInfo, if the item
                    // D-Bus interface name specified in FRU_Master.json does
                    // not have corresponding config jsons
                    try
                    {
                        auto recordInfos =
                            handle.getRecordInfo(interface.first);
                        populateRecords(interfaces, recordInfos);
                    }
                    catch (const std::exception& e)
                    {
                        std::cout << "Config JSONs missing for the item "
                                     "interface type"
                                  << interface.first;
                        break;
                    }
                }
            }
        }
    }

    if (table.size())
    {
        padBytes = utils::getNumPadBytes(table.size());
        table.resize(table.size() + padBytes, 0);

        // Calculate the checksum
        boost::crc_32_type result;
        result.process_bytes(table.data(), table.size());
        checksum = result.checksum();
    }
}

void FruImpl::populateRecords(
    const pldm::responder::dbus::InterfaceMap& interfaces,
    const fru_parser::FruRecordInfos& recordInfos)
{
    // recordSetIdentifier for the FRU will be set when the first record gets
    // added for the FRU
    uint16_t recordSetIdentifier = 0;
    auto numRecsCount = numRecs;

    for (auto const& [recType, encType, fieldInfos] : recordInfos)
    {
        std::vector<uint8_t> tlvs;
        uint8_t numFRUFields = 0;
        for (auto const& [intf, prop, propType, fieldTypeNum] : fieldInfos)
        {
            try
            {
                auto propValue = interfaces.at(intf).at(prop);
                if (propType == "bytearray")
                {
                    auto byteArray = std::get<std::vector<uint8_t>>(propValue);
                    if (!byteArray.size())
                    {
                        continue;
                    }

                    numFRUFields++;
                    tlvs.emplace_back(fieldTypeNum);
                    tlvs.emplace_back(byteArray.size());
                    std::move(std::begin(byteArray), std::end(byteArray),
                              std::back_inserter(tlvs));
                }
                else if (propType == "string")
                {
                    auto str = std::get<std::string>(propValue);
                    if (!str.size())
                    {
                        continue;
                    }

                    numFRUFields++;
                    tlvs.emplace_back(fieldTypeNum);
                    tlvs.emplace_back(str.size());
                    std::move(std::begin(str), std::end(str),
                              std::back_inserter(tlvs));
                }
            }
            catch (const std::out_of_range& e)
            {
                std::cout << "Interface = " << intf
                          << " , and Property = " << prop
                          << " , in config json not present in D-Bus";
                continue;
            }
        }

        if (tlvs.size())
        {
            if (numRecs == numRecsCount)
            {
                recordSetIdentifier = nextRSI();
            }
            auto curSize = table.size();
            table.resize(curSize + recHeaderSize + tlvs.size());
            encode_fru_record(table.data(), table.size(), &curSize,
                              recordSetIdentifier, recType, numFRUFields,
                              encType, tlvs.data(), tlvs.size());
            numRecs++;
        }
    }
}

void FruImpl::getFRUTable(Response& response)
{
    auto hdrSize = response.size();

    response.resize(hdrSize + table.size() + sizeof(checksum), 0);
    std::copy(table.begin(), table.end(), response.begin() + hdrSize);

    // Copy the checksum to response data
    auto iter = response.begin() + hdrSize + table.size();
    std::copy_n(reinterpret_cast<const uint8_t*>(&checksum), sizeof(checksum),
                iter);
}

namespace fru
{

Response Handler::getFRURecordTableMetadata(const pldm_msg* request,
                                            size_t /*payloadLength*/)
{
    constexpr uint8_t major = 0x01;
    constexpr uint8_t minor = 0x00;
    constexpr uint32_t maxSize = 0xFFFFFFFF;

    Response response(sizeof(pldm_msg_hdr) +
                          PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    encode_get_fru_record_table_metadata_resp(
        request->hdr.instance_id, PLDM_SUCCESS, major, minor, maxSize,
        impl.size(), impl.numRSI(), impl.numRecords(), impl.checkSum(),
        responsePtr);

    return response;
}

Response Handler::getFRURecordTable(const pldm_msg* request,
                                    size_t payloadLength)
{
    if (payloadLength != PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES)
    {
        return ccOnlyResponse(request, PLDM_ERROR_INVALID_LENGTH);
    }

    Response response(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    encode_get_fru_record_table_resp(request->hdr.instance_id, PLDM_SUCCESS, 0,
                                     PLDM_START_AND_END, responsePtr);

    impl.getFRUTable(response);

    return response;
}

} // namespace fru

} // namespace responder

} // namespace pldm
