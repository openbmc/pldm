#include <string.h>

#include <cstring>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/bios.h"
#include "libpldm/bios_table.h"

#include <gtest/gtest.h>

TEST(AttrEntryEnum, DecodeTest)
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

    uint8_t pvNumber;
    auto entry =
        reinterpret_cast<struct pldm_bios_attr_table_entry*>(enumEntry.data());
    pldm_bios_table_attr_entry_enum_decode_pv_num(entry, &pvNumber);
    EXPECT_EQ(pvNumber, 2);
    std::vector<uint16_t> pvHdls(pvNumber, 0);
    pldm_bios_table_attr_entry_enum_decode_pv_hdls(entry, pvHdls.data(),
                                                   pvHdls.size());
    EXPECT_EQ(pvHdls[0], 2);
    EXPECT_EQ(pvHdls[1], 3);
    uint8_t defNumber;
    pldm_bios_table_attr_entry_enum_decode_def_num(entry, &defNumber);
    EXPECT_EQ(defNumber, 1);
}

TEST(AttrValueEntryEnum, EncodeTest)
{
    std::vector<uint8_t> attrValueEntry{
        1, 0, /* attr handle */
        0,    /* attr type */
        2,    /* number of current value */
        0,    /* index of string handles */
        1,    /* index of string handles */
    };

    auto size = pldm_bios_table_attr_value_entry_encode_enum_length(2);
    std::vector<uint8_t> entry(size, 0);

    std::vector<uint8_t> hdls{0, 1};
    auto rc = pldm_bios_table_attr_value_entry_encode_enum(
        entry.data(), entry.size(), 1, 0, 2, hdls.data());
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(entry, attrValueEntry);
}

TEST(AttrValueStringEnum, EncodeTest)
{
    std::vector<uint8_t> attrValueEntry{
        1,   0,   /* attr handle */
        1,        /* attr type */
        2,   0,   /* length of current string */
        'e', 'f', /* current string */
    };

    const char* str = "ef";
    auto size = pldm_bios_table_attr_value_entry_encode_string_length(2);
    std::vector<uint8_t> entry(size, 0);
    auto rc = pldm_bios_table_attr_value_entry_encode_string(
        entry.data(), entry.size(), 1, 1, 2, str);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(entry, attrValueEntry);
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
    std::vector<uint8_t> table;
    table.insert(table.end(), enumEntry.begin(), enumEntry.end());
    table.insert(table.end(), stringEntry.begin(), stringEntry.end());
    auto padSize = ((table.size() % 4) ? (4 - table.size() % 4) : 0);

    table.insert(table.end(), padSize, 0);
    table.insert(table.end(), sizeof(uint32_t) /*checksum*/, 0);

    auto iter = pldm_bios_table_iter_create(table.data(), table.size());
    auto entry = pldm_bios_table_attr_entry_next(iter);
    auto rc = std::memcmp(entry, enumEntry.data(), enumEntry.size());
    EXPECT_EQ(rc, 0);
    entry = pldm_bios_table_attr_entry_next(iter);
    rc = std::memcmp(entry, stringEntry.data(), stringEntry.size());
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(pldm_bios_table_attr_iter_is_end(iter));
    entry = pldm_bios_table_attr_entry_next(iter);
    EXPECT_EQ(entry, nullptr);
    pldm_bios_table_iter_free(iter);
}