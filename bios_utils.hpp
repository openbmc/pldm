#pragma once

#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

#include "libpldm/bios_table.h"

namespace pldm
{
namespace bios
{
namespace utils
{

using Table = std::vector<uint8_t>;

template <pldm_bios_table_types N>
class BIOSTableIter
{
  public:
    struct EndSentinel
    {
    };
    class iterator
    {
      public:
        using T = typename std::conditional<
            N == PLDM_BIOS_STRING_TABLE, pldm_bios_string_table_entry,
            typename std::conditional<
                N == PLDM_BIOS_ATTR_TABLE, pldm_bios_attr_table_entry,
                typename std::conditional<N == PLDM_BIOS_ATTR_VAL_TABLE,
                                          pldm_bios_attr_val_table_entry,
                                          void>::type>::type>::type;

        explicit iterator(const void* data, size_t length) noexcept :
            iter(pldm_bios_table_iter_create(
                     data, length, static_cast<pldm_bios_table_types>(N)),
                 pldm_bios_table_iter_free)
        {
        }

        const T* operator*()
        {
            return (const T*)pldm_bios_table_iter_value(iter.get());
        }

        iterator& operator++()
        {
            pldm_bios_table_iter_next(iter.get());
            return *this;
        }

        bool operator==(const EndSentinel&)
        {
            return pldm_bios_table_iter_is_end(iter.get());
        }

        bool operator!=(const EndSentinel& endSentinel)
        {
            return !operator==(endSentinel);
        }

      private:
        std::unique_ptr<pldm_bios_table_iter,
                        decltype(&pldm_bios_table_iter_free)>
            iter;
    };

    BIOSTableIter(const void* data, size_t length) noexcept :
        tableData(data), tableSize(length)
    {
    }

    iterator begin()
    {
        return iterator(tableData, tableSize);
    }

    EndSentinel end()
    {
        return {};
    }

  private:
    const void* tableData;
    size_t tableSize;
};

} // namespace utils
} // namespace bios
} // namespace pldm
