#include "fru.hpp"

#include "utils.hpp"

#include <boost/crc.hpp>
#include <cstring>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>

namespace pldm
{

namespace responder
{

namespace internal
{

FruImpl table(FRU_JSONS_DIR);
FruIntf<FruImpl> intf(table);

} // namespace internal

namespace fru
{

Response Handler::getFRURecordTableMetadata(const pldm_msg* request,
                                            size_t /*payloadLength*/)
{
    using namespace internal;
    constexpr uint8_t major = 0x01;
    constexpr uint8_t minor = 0x00;
    constexpr uint32_t maxSize = 0xFFFFFFFF;

    Response response(sizeof(pldm_msg_hdr) +
                          PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    sd_journal_print(LOG_INFO, "RSI = %d, Recs = %d, Size = %d", intf.numRSI(),
                     intf.numRecords(), intf.size());

    encode_get_fru_record_table_metadata_resp(
        request->hdr.instance_id, PLDM_SUCCESS, major, minor, maxSize,
        intf.size(), intf.numRSI(), intf.numRecords(), intf.checkSum(),
        responsePtr);

    return response;
}

Response Handler::getFRURecordTable(const pldm_msg* request,
                                    size_t payloadLength)
{
    using namespace internal;
    Response response(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES)
    {
        encode_get_fru_record_table_resp(request->hdr.instance_id,
                                         PLDM_ERROR_INVALID_LENGTH, 0,
                                         PLDM_START_AND_END, responsePtr);
        return response;
    }

    encode_get_fru_record_table_resp(request->hdr.instance_id, PLDM_SUCCESS, 0,
                                     PLDM_START_AND_END, responsePtr);

    intf.getFRUTable(response);

    return response;
}

} // namespace fru


FruImpl::FruImpl(const std::string& configPath)
{

    fru_parser::FruParser handle;

    try
    {
        handle = fru_parser::get(configPath);
    }
    catch (const std::exception& e)
    {
        return;
    }

    auto dbusInfo = handle.inventoryLookup();

    // Read the all the inventory D-Bus objects
    auto bus = sdbusplus::bus::new_default();
    auto method = bus.new_method_call(
        std::get<0>(dbusInfo).c_str(), std::get<1>(dbusInfo).c_str(),
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    auto reply = bus.call(method);

    ObjectValueTree objects;
    reply.read(objects);

    // Populate all the interested Item types to a map for easy lookup
    std::map<dbus::Interface, uint8_t> itemIntfsMap;
    auto itemIntfs = std::get<2>(dbusInfo);
    std::transform(std::begin(itemIntfs), std::end(itemIntfs),
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
                    // Add try-catch for getRecordInfo
                    auto recordInfos = handle.getRecordInfo(interface.first);
                    populateRecord(interfaces, recordInfos);
                }
            }
        }
    }

    std::cout << "Table Size" << table.size();

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

void FruImpl::populateRecord(
    const pldm::responder::DbusInterfaceMap& interfaces,
    const fru_parser::FruRecordInfos& recordInfos)
{
    auto recordSetIdentifier = nextRSI();

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
                    tlvs.insert(std::end(tlvs), std::begin(byteArray),
                                std::end(byteArray));
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
                    tlvs.insert(std::end(tlvs), std::begin(str), std::end(str));
                }
            }
            catch (const std::out_of_range& e)
            {
                continue;
            }
        }

        if (tlvs.size())
        {
            auto curSize = table.size();
            table.resize(curSize + recHeaderSize + tlvs.size());
            encode_fru_record(table.data(), table.size(), &curSize,
                              recordSetIdentifier, recType, numFRUFields,
                              encType, tlvs.data(), tlvs.size());
            numrecs++;
        }
    }
}

void FruImpl::getFRUTable(Response& response)
{
    using namespace phosphor::logging;

    std::ostringstream tempStream;
    tempStream << "Table Data: ";
    if (!table.empty())
    {
        for (int byte : table)
        {
            tempStream << std::setfill('0') << std::setw(2) << std::hex << byte
                       << " ";
        }
    }
    log<level::INFO>(tempStream.str().c_str());

    auto hdrSize = response.size();

    response.resize(hdrSize + table.size() + sizeof(checksum), 0);
    std::copy(table.begin(), table.end(), response.begin() + hdrSize);

    // Copy the checksum to response data
    auto iter = response.begin() + hdrSize + table.size();
    std::copy_n(reinterpret_cast<const uint8_t*>(&checksum), sizeof(checksum),
                iter);
}

} // namespace responder

} // namespace pldm
