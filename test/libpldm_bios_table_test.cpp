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

TEST(AttrTable, EnumEntryEncodeTest)
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

    std::vector<uint16_t> pv_hdls{2, 3};
    std::vector<uint8_t> defs{0};

    struct pldm_bios_table_attr_entry_enum_info info = {
        1,              /* name handle */
        false,          /* read only */
        2,              /* pv number */
        pv_hdls.data(), /* pv handle */
        1,              /*def number */
        defs.data()     /*def index*/
    };
    auto encodeLength = pldm_bios_table_attr_entry_enum_encode_length(2, 1);
    EXPECT_EQ(encodeLength, enumEntry.size());

    std::vector<uint8_t> encodeEntry(encodeLength, 0);
    pldm_bios_table_attr_entry_enum_encode(encodeEntry.data(),
                                           encodeEntry.size(), &info);
    // set attr handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(enumEntry, encodeEntry);
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

TEST(StringTable, EntryEncodeTest)
{
    std::vector<uint8_t> stringEntry{
        0,   0,                            /* string handle*/
        7,   0,                            /* string length */
        'A', 'l', 'l', 'o', 'w', 'e', 'd', /* string */
    };

    const char* str = "Allowed";
    auto str_length = std::strlen(str);
    auto encodeLength = pldm_bios_table_string_entry_encode_length(str_length);
    EXPECT_EQ(encodeLength, stringEntry.size());

    std::vector<uint8_t> encodeEntry(encodeLength, 0);
    pldm_bios_table_string_entry_encode(encodeEntry.data(), encodeEntry.size(),
                                        str, str_length);
    // set string handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(stringEntry, encodeEntry);
}

TEST(StringTable, EntryDecodeTest)
{
    std::vector<uint8_t> stringEntry{
        4,   0,                            /* string handle*/
        7,   0,                            /* string length */
        'A', 'l', 'l', 'o', 'w', 'e', 'd', /* string */
    };
    auto entry = reinterpret_cast<struct pldm_bios_string_table_entry*>(
        stringEntry.data());
    auto handle = pldm_bios_table_string_entry_decode_handle(entry);
    EXPECT_EQ(handle, 4);
    auto str_length = pldm_bios_table_string_entry_decode_string_length(entry);
    EXPECT_EQ(str_length, 7);

    std::vector<char> buffer(str_length + 1, 0);
    auto rc = pldm_bios_table_string_entry_decode_string(entry, buffer.data(),
                                                         buffer.size());
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(std::strcmp("Allowed", buffer.data()), 0);
}

TEST(StringTable, IteratorTest)
{
    std::vector<uint8_t> stringHello{
        0,   0,                  /* string handle*/
        5,   0,                  /* string length */
        'H', 'e', 'l', 'l', 'o', /* string */
    };
    std::vector<uint8_t> stringWorld{
        1,   0,                       /* string handle*/
        6,   0,                       /* string length */
        'W', 'o', 'r', 'l', 'd', '!', /* string */
    };

    Table table;
    buildTable(table, stringHello, stringWorld);

    auto iter = pldm_bios_table_iter_create(table.data(), table.size(),
                                            PLDM_BIOS_STRING_TABLE);
    auto entry = pldm_bios_table_string_entry_next(iter);
    auto rc = std::memcmp(entry, stringHello.data(), stringHello.size());
    EXPECT_EQ(rc, 0);
    entry = pldm_bios_table_string_entry_next(iter);
    rc = std::memcmp(entry, stringWorld.data(), stringWorld.size());
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(pldm_bios_table_iter_is_end(iter));
    entry = pldm_bios_table_string_entry_next(iter);
    EXPECT_EQ(entry, nullptr);
    pldm_bios_table_iter_free(iter);
}

TEST(StringTable, FindTest)
{
    std::vector<uint8_t> stringHello{
        1,   0,                  /* string handle*/
        5,   0,                  /* string length */
        'H', 'e', 'l', 'l', 'o', /* string */
    };
    std::vector<uint8_t> stringWorld{
        2,   0,                       /* string handle*/
        6,   0,                       /* string length */
        'W', 'o', 'r', 'l', 'd', '!', /* string */
    };
    std::vector<uint8_t> stringHi{
        3,   0,   /* string handle*/
        2,   0,   /* string length */
        'H', 'i', /* string */
    };

    Table table;
    buildTable(table, stringHello, stringWorld, stringHi);

    auto entry = pldm_bios_table_string_find_by_string(table.data(),
                                                       table.size(), "World!");
    EXPECT_NE(entry, nullptr);
    auto handle = pldm_bios_table_string_entry_decode_handle(entry);
    EXPECT_EQ(handle, 2);

    entry = pldm_bios_table_string_find_by_string(table.data(), table.size(),
                                                  "Worl");
    EXPECT_EQ(entry, nullptr);

    entry =
        pldm_bios_table_string_find_by_handle(table.data(), table.size(), 3);
    EXPECT_NE(entry, nullptr);
    auto str_length = pldm_bios_table_string_entry_decode_string_length(entry);
    EXPECT_EQ(str_length, 2);
    std::vector<char> strBuf(str_length + 1, 0);
    auto rc = pldm_bios_table_string_entry_decode_string(entry, strBuf.data(),
                                                         strBuf.size());
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(std::strcmp("Hi", strBuf.data()), 0);

    entry =
        pldm_bios_table_string_find_by_handle(table.data(), table.size(), 4);
    EXPECT_EQ(entry, nullptr);
}