#include <string.h>

#include <cstring>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/bios.h"
#include "libpldm/bios_table.h"

#include <gtest/gtest.h>

using Table = std::vector<uint8_t>;

void buildTable(Table& table)
{
    auto padSize = ((table.size() % 4) ? (4 - table.size() % 4) : 0);
    table.insert(table.end(), padSize, 0);
    table.insert(table.end(), sizeof(uint32_t) /*checksum*/, 0);
}

template <typename First, typename... Rest>
void buildTable(Table& table, First& first, Rest&... rest)
{
    table.insert(table.end(), first.begin(), first.end());
    buildTable(table, rest...);
}

TEST(AttrTable, EnumEntryDecodeTest)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        1, 0, /* attr name handle */
        2,    /* number of possible value */
        2, 0, /* possible value handle */
        3, 0, /* possible value handle */
        1,    /* number of default value */
        0     /* defaut value string handle index */
    };

    auto entry =
        reinterpret_cast<struct pldm_bios_attr_table_entry*>(enumEntry.data());
    uint8_t pvNumber = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
    EXPECT_EQ(pvNumber, 2);
    uint8_t defNumber = pldm_bios_table_attr_entry_enum_decode_def_num(entry);
    EXPECT_EQ(defNumber, 1);

    pvNumber = 0;
    auto rc =
        pldm_bios_table_attr_entry_enum_decode_pv_num_check(entry, &pvNumber);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(pvNumber, 2);
    defNumber = 0;
    rc =
        pldm_bios_table_attr_entry_enum_decode_def_num_check(entry, &defNumber);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(defNumber, 1);

    entry->attr_type = PLDM_BIOS_STRING;
    rc = pldm_bios_table_attr_entry_enum_decode_pv_num_check(entry, &pvNumber);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc =
        pldm_bios_table_attr_entry_enum_decode_def_num_check(entry, &defNumber);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(AttrTable, StringEntryDecodeTest)
{
    std::vector<uint8_t> stringEntry{
        1,   0,       /* attr handle */
        1,            /* attr type */
        12,  0,       /* attr name handle */
        1,            /* string type */
        1,   0,       /* minimum length of the string in bytes */
        100, 0,       /* maximum length of the string in bytes */
        3,   0,       /* length of default string in length */
        'a', 'b', 'c' /* default string  */
    };

    auto entry = reinterpret_cast<struct pldm_bios_attr_table_entry*>(
        stringEntry.data());
    uint16_t def_string_length =
        pldm_bios_table_attr_entry_string_decode_def_string_length(entry);
    EXPECT_EQ(def_string_length, 3);

    def_string_length = 0;
    auto rc = pldm_bios_table_attr_entry_string_decode_def_string_length_check(
        entry, &def_string_length);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(def_string_length, 3);

    entry->attr_type = PLDM_BIOS_INTEGER;
    rc = pldm_bios_table_attr_entry_string_decode_def_string_length_check(
        entry, &def_string_length);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(AttrTable, ItearatorTest)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        1, 0, /* attr name handle */
        2,    /* number of possible value */
        2, 0, /* possible value handle */
        3, 0, /* possible value handle */
        1,    /* number of default value */
        0     /* defaut value string handle index */
    };
    std::vector<uint8_t> stringEntry{
        1,   0,       /* attr handle */
        1,            /* attr type */
        12,  0,       /* attr name handle */
        1,            /* string type */
        1,   0,       /* minimum length of the string in bytes */
        100, 0,       /* maximum length of the string in bytes */
        3,   0,       /* length of default string in length */
        'a', 'b', 'c' /* default string  */
    };

    Table table;
    buildTable(table, enumEntry, stringEntry, enumEntry);
    auto iter = pldm_bios_table_iter_create(table.data(), table.size(),
                                            PLDM_BIOS_ATTR_TABLE);
    auto entry = pldm_bios_table_iter_attr_entry_value(iter);
    auto rc = std::memcmp(entry, enumEntry.data(), enumEntry.size());
    EXPECT_EQ(rc, 0);

    pldm_bios_table_iter_next(iter);
    entry = pldm_bios_table_iter_attr_entry_value(iter);
    rc = std::memcmp(entry, stringEntry.data(), stringEntry.size());
    EXPECT_EQ(rc, 0);

    pldm_bios_table_iter_next(iter);
    entry = pldm_bios_table_iter_attr_entry_value(iter);
    rc = std::memcmp(entry, enumEntry.data(), enumEntry.size());
    EXPECT_EQ(rc, 0);

    pldm_bios_table_iter_next(iter);
    EXPECT_TRUE(pldm_bios_table_iter_is_end(iter));
    pldm_bios_table_iter_free(iter);
}

TEST(Itearator, DeathTest)
{

    Table table(256, 0);

    /* first entry */
    auto attr_entry =
        reinterpret_cast<struct pldm_bios_attr_table_entry*>(table.data());
    auto iter = pldm_bios_table_iter_create(table.data(), table.size(),
                                            PLDM_BIOS_ATTR_TABLE);
    attr_entry->attr_type = PLDM_BIOS_PASSWORD;
    EXPECT_DEATH(pldm_bios_table_iter_next(iter), "attr_table_entry != NULL");
    attr_entry->attr_type = PLDM_BIOS_INTEGER;
    EXPECT_DEATH(pldm_bios_table_iter_next(iter), "attr_table_entry != NULL");
    pldm_bios_table_iter_free(iter);

    EXPECT_DEATH(pldm_bios_table_iter_create(table.data(), table.size(),
                                             PLDM_BIOS_STRING_TABLE),
                 "iter->entry_length_handler != NULL");
    EXPECT_DEATH(pldm_bios_table_iter_create(table.data(), table.size(),
                                             PLDM_BIOS_ATTR_VAL_TABLE),
                 "iter->entry_length_handler != NULL");
}
