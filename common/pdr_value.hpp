#pragma once

#include <libpldm/platform.h>

#include <cstdint>
#include <limits>

namespace pldm
{
namespace utils
{

/** @brief Read a range field of a numeric sensor or numeric effecter PDR
 *
 *  The nominalValue, normalMax, normalMin, ratedMax, ratedMin and threshold
 *  fields of a numeric PDR are unions whose active alternative is selected by
 *  the PDR's rangeFieldFormat. Refer to DSP0248 v1.2.0 Table 87.
 *
 *  @param[in] format - rangeFieldFormat of the PDR owning the field
 *  @param[in] value - the range field to read
 *
 *  @return the field value, or NaN if the format is not a defined value
 */
inline double getRangeFieldValue(uint8_t format,
                                 const union_range_field_format& value)
{
    switch (format)
    {
        case PLDM_RANGE_FIELD_FORMAT_UINT8:
            return static_cast<double>(value.value_u8);
        case PLDM_RANGE_FIELD_FORMAT_SINT8:
            return static_cast<double>(value.value_s8);
        case PLDM_RANGE_FIELD_FORMAT_UINT16:
            return static_cast<double>(value.value_u16);
        case PLDM_RANGE_FIELD_FORMAT_SINT16:
            return static_cast<double>(value.value_s16);
        case PLDM_RANGE_FIELD_FORMAT_UINT32:
            return static_cast<double>(value.value_u32);
        case PLDM_RANGE_FIELD_FORMAT_SINT32:
            return static_cast<double>(value.value_s32);
        case PLDM_RANGE_FIELD_FORMAT_REAL32:
            return static_cast<double>(value.value_f32);
        default:
            return std::numeric_limits<double>::quiet_NaN();
    }
}

} // namespace utils
} // namespace pldm
