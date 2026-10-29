#pragma once

#include <libpldm/edac.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace pldm::platform_mc::file_transfer
{
// DSP0240 1.2.0 §9.6.2: whole PLDM message, not data bytes or MCTP MTU.
inline std::optional<uint16_t> negotiatePartSize(
    uint16_t offered, uint16_t returned, bool fileTypeSupported)
{
    if (!fileTypeSupported || offered < 256 || returned < 256 ||
        !std::has_single_bit(offered) || !std::has_single_bit(returned))
    {
        return std::nullopt;
    }
    return std::min(offered, returned);
}

inline std::optional<uint32_t> validFileSize(double reading)
{
    if (!std::isfinite(reading) || reading < 0 || reading > UINT32_MAX ||
        std::trunc(reading) != reading)
    {
        return std::nullopt;
    }
    return static_cast<uint32_t>(reading);
}

// Subtraction avoids both offset+length overflow and silent uint32 narrowing.
inline std::optional<uint32_t> readLength(uint32_t size, uint64_t offset,
                                          uint64_t requested)
{
    if (offset > size || requested > size - offset)
    {
        return std::nullopt;
    }
    return requested == 0 ? size - static_cast<uint32_t>(offset)
                          : static_cast<uint32_t>(requested);
}

/** One bounded regular-file section; DSP0240 §9.6.6 Figure 4/Table 17.
 * Wire codecs and transport ownership stay in libpldm/the requester.
 * Bytes are released to the sink only after the section's completion ACK.
 */
class Section
{
  public:
    static constexpr uint32_t maximumSection = 64 * 1024; // local memory policy
    enum class Outcome
    {
        accepted,
        retry,
        complete,
        invalid
    };

    Section(uint32_t length, uint16_t partSize) :
        length(length), capacity(partSize > 17 ? partSize - 17 : 0)
    {}

    uint8_t operation() const
    {
        return op;
    }
    uint32_t handle() const
    {
        return currentHandle;
    }
    std::span<const uint8_t> bytes() const
    {
        return data;
    }

    Outcome accept(uint8_t flag, uint32_t nextHandle,
                   std::span<const uint8_t> part, uint32_t checksum)
    {
        if (finished || length == 0 || length > maximumSection || capacity == 0)
        {
            return Outcome::invalid;
        }
        if (op == 3) // XFER_COMPLETE only accepts an empty completion ACK.
        {
            if (flag != 8 || !part.empty() || nextHandle != 0)
            {
                return Outcome::invalid;
            }
            finished = true;
            return Outcome::complete;
        }
        const bool first = data.empty();
        const bool end = flag == 4 || flag == 5;
        if ((first && flag != 1 && flag != 5) ||
            (!first && flag != 2 && flag != 4) || part.empty() ||
            part.size() > capacity || part.size() > length - data.size() ||
            (!end && (part.size() != capacity || nextHandle == 0)) ||
            (end && nextHandle != 0))
        {
            return Outcome::invalid;
        }
        // Use the public CRC API available in the Phoenix-pinned libpldm.
        // The section buffer is bounded, including bytes awaiting validation.
        const auto previousSize = data.size();
        data.insert(data.end(), part.begin(), part.end());
        const auto candidate = pldm_edac_crc32(data.data(), data.size());
        data.resize(previousSize);
        if (candidate != checksum)
        {
            if (++retries > 3) // local robustness limit, not a DMTF constant
            {
                return Outcome::invalid;
            }
            op = 4; // CURRENT_PART retains the handle used for the bad part.
            return Outcome::retry;
        }
        if ((end && data.size() + part.size() != length) ||
            (!end && data.size() + part.size() == length))
        {
            return Outcome::invalid;
        }
        data.insert(data.end(), part.begin(), part.end());
        retries = 0;
        op = end ? 3 : 1;
        currentHandle = end ? 0 : nextHandle;
        return Outcome::accepted;
    }

  private:
    uint32_t length;
    size_t capacity;
    std::vector<uint8_t> data;
    uint32_t currentHandle = 0;
    uint8_t op = 0;
    unsigned retries = 0;
    bool finished = false;
};
} // namespace pldm::platform_mc::file_transfer
