#include <string>

#include "pldmd/pldm_fru_parser.cpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::utils;

namespace pldm
{
namespace fruparser
{

template <typename T>
class MockPLDMFRUParser : public PLDMFRUParser<FruTableIntf>
{
  public:
    MockPLDMFRUParser(sdbusplus::bus::bus& bus, const std::string& objPath,
                      DBusHandler* const dbusHandler) :
        PLDMFRUParser<FruTableIntf>(bus, objPath, dbusHandler)
    {}
};

class TestFRUParser : public testing::Test
{
  public:
    sdbusplus::bus::bus bus;
    pldm::utils::DBusHandler* dbusHandler;
    MockPLDMFRUParser<FruTableIntf> mockfruparser;
    TestFRUParser() :
        bus(sdbusplus::bus::new_default()),
        mockfruparser(bus,
                      "/xyz/openbmc_project/pldm/fru_record_set/" +
                          std::to_string(10),
                      dbusHandler)
    {}
    ~TestFRUParser()
    {}
};

TEST_F(TestFRUParser, testTableParsed)
{
    uint16_t recordSetIdA = 10;
    uint8_t recordTypeA = PLDM_FRU_RECORD_TYPE_GENERAL;
    uint8_t numberFruFeildA = 2;
    uint8_t encodingTypeA = PLDM_FRU_ENCODING_ASCII;
    uint8_t typeA1 = PLDM_FRU_FIELD_TYPE_CHASSIS;
    std::string A1 = "ChassisType";
    const std::string valueA1("E980");
    uint8_t lengthA1 = valueA1.size();
    uint8_t typeA2 = PLDM_FRU_FIELD_TYPE_MODEL;
    std::string A2 = "Model";
    const std::string valueA2("007");
    uint8_t lengthA2 = valueA2.size();

    uint16_t recordSetIdB = 11;
    uint8_t recordTypeB = PLDM_FRU_RECORD_TYPE_GENERAL;
    uint8_t numberFruFeildB = 2;
    uint8_t encodingTypeB = PLDM_FRU_ENCODING_ASCII;
    uint8_t typeB1 = PLDM_FRU_FIELD_TYPE_MANUFAC;
    std::string B1 = "Manufacturer";
    const std::string valueB1("IBM");
    uint8_t lengthB1 = valueB1.size();
    uint8_t typeB2 = PLDM_FRU_FIELD_TYPE_VENDOR;
    std::string B2 = "Vendor";
    const std::string valueB2("ISDL");
    uint8_t lengthB2 = valueB2.size();

    size_t fruTableLength =
        sizeof(recordSetIdA) + sizeof(recordTypeA) + sizeof(numberFruFeildA) +
        sizeof(encodingTypeA) + sizeof(typeA1) + valueA1.size() +
        sizeof(lengthA1) + sizeof(typeA2) + valueA2.size() + sizeof(lengthA2) +
        sizeof(recordSetIdB) + sizeof(recordTypeB) + sizeof(numberFruFeildB) +
        sizeof(encodingTypeB) + sizeof(typeB1) + valueB1.size() +
        sizeof(lengthB1) + sizeof(typeB2) + valueB2.size() + sizeof(lengthB2);

    std::array<uint8_t, 100> fruRecordTable{};

    auto fruRecordTableData1 =
        reinterpret_cast<struct pldm_fru_record_data_format*>(
            fruRecordTable.data());

    fruRecordTableData1->record_set_id = recordSetIdA;
    fruRecordTableData1->record_type = recordTypeA;
    fruRecordTableData1->num_fru_fields = numberFruFeildA;
    fruRecordTableData1->encoding_type = encodingTypeA;

    auto tlvs1 = reinterpret_cast<struct pldm_fru_record_tlv*>(
        fruRecordTableData1->tlvs);

    tlvs1->type = typeA1;
    tlvs1->length = lengthA1;
    memcpy(tlvs1->value, &valueA1, lengthA1);

    auto tlvs2 =
        reinterpret_cast<struct pldm_fru_record_tlv*>(tlvs1->value + lengthA1);

    tlvs2->type = typeA2;
    tlvs2->length = lengthA2;
    memcpy(tlvs2->value, &valueA2, lengthA2);

    auto fruRecordTableData2 =
        reinterpret_cast<struct pldm_fru_record_data_format*>(tlvs2->value +
                                                              lengthA2);

    fruRecordTableData2->record_set_id = recordSetIdB;
    fruRecordTableData2->record_type = recordTypeB;
    fruRecordTableData2->num_fru_fields = numberFruFeildB;
    fruRecordTableData2->encoding_type = encodingTypeB;

    auto tlvs3 = reinterpret_cast<struct pldm_fru_record_tlv*>(
        fruRecordTableData2->tlvs);

    tlvs3->type = typeB1;
    tlvs3->length = lengthB1;
    memcpy(tlvs3->value, &valueB1, lengthB1);
    auto tlvs4 =
        reinterpret_cast<struct pldm_fru_record_tlv*>(tlvs3->value + lengthB1);

    tlvs4->type = typeB2;
    tlvs4->length = lengthB2;
    memcpy(tlvs4->value, &valueB2, lengthB2);

    std::string objPath = "/xyz/openbmc_project/pldm/fru_record_set/";
    std::string interface = "xyz.openbmc_project.Inventory.Source.PLDM.FRU";

    auto objPathA = objPath;
    objPathA.append(std::to_string(recordSetIdA));

    auto rc = mockfruparser.parseFRUTable(
        reinterpret_cast<const uint8_t*>(fruRecordTable.data()), fruTableLength,
        0, dbusHandler);

    EXPECT_EQ(rc, PLDM_SUCCESS);

    auto objsVec = mockfruparser.getFRUParserObjs();

    /*uint8_t iter = 0;

    auto recordDataValRetA1 =
        objsVec[iter]->dbusHandler->getDbusProperty<std::string>(
            objPathA.c_str(), A1.c_str(), interface.c_str());
    EXPECT_EQ(recordDataValRetA1, valueA1);
    iter++;*/
}

} // namespace fruparser
} // namespace pldm
