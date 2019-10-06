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
    std::vector<uint16_t> pvHandles(pvNumber, 0);
    auto rc = pldm_bios_table_attr_entry_enum_decode_pv_hdls(
        entry, pvHandles.data(), pvNumber);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(pvHandles[0], 2);
    EXPECT_EQ(pvHandles[1], 3);
    uint8_t defNumber = pldm_bios_table_attr_entry_enum_decode_def_num(entry);
    EXPECT_EQ(defNumber, 1);
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
        4,   0,       /* attr handle */
        1,            /* attr type */
        12,  0,       /* attr name handle */
        1,            /* string type */
        1,   0,       /* minimum length of the string in bytes */
        100, 0,       /* maximum length of the string in bytes */
        3,   0,       /* length of default string in length */
        'a', 'b', 'c' /* default string  */
    };

    Table table;
    buildTable(table, enumEntry, stringEntry);
    auto iter = pldm_bios_table_iter_create(table.data(), table.size(),
                                            PLDM_BIOS_ATTR_TABLE);
    auto entry = pldm_bios_table_attr_entry_next(iter);
    auto rc = std::memcmp(entry, enumEntry.data(), enumEntry.size());
    EXPECT_EQ(rc, 0);
    entry = pldm_bios_table_attr_entry_next(iter);
    rc = std::memcmp(entry, stringEntry.data(), stringEntry.size());
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(pldm_bios_table_iter_is_end(iter));
    entry = pldm_bios_table_attr_entry_next(iter);
    EXPECT_EQ(entry, nullptr);
    pldm_bios_table_iter_free(iter);
}
