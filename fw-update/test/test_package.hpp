#pragma once

#include <libpldm/edac.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace pldm::fw_update::test
{

// Extend test_pkg with a second component (identifier 200, VersionString4).
// Distinct payloads detect wrong image offsets; the bitmap can exclude it.
inline std::vector<uint8_t> twoComponentPackage(uint8_t applicable = 3)
{
    std::ifstream source("./test_pkg", std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(source), {}};
    constexpr size_t headerSize = 139;
    constexpr size_t componentInfoOffset = 99;
    constexpr size_t componentInfoSize = 36;
    constexpr size_t payloadSize = 1024;
    constexpr size_t newHeaderSize = headerSize + componentInfoSize;
    if (bytes.size() != headerSize + payloadSize)
    {
        throw std::runtime_error("Unexpected test_pkg layout");
    }

    std::vector<uint8_t> second(bytes.begin() + componentInfoOffset,
                                bytes.begin() + headerSize - 4);
    bytes.resize(headerSize - 4); // Replace the old header checksum
    bytes.insert(bytes.end(), second.begin(), second.end());
    auto setLE = [&](size_t offset, uint32_t value, size_t width) {
        for (size_t i = 0; i < width; ++i)
        {
            bytes.at(offset + i) = (value >> (8 * i)) & 0xff;
        }
    };
    setLE(17, newHeaderSize, 2); // PackageHeaderSize
    setLE(62, applicable, 1);    // ApplicableComponents bitmap
    setLE(97, 2, 2);             // ComponentImageCount
    setLE(componentInfoOffset + 12, newHeaderSize, 4);
    setLE(componentInfoOffset + componentInfoSize + 2, 200, 2);
    setLE(componentInfoOffset + componentInfoSize + 12,
          newHeaderSize + payloadSize, 4);
    bytes.at(componentInfoOffset + 2 * componentInfoSize - 1) = '4';
    const auto checksum = pldm_edac_crc32(bytes.data(), bytes.size());
    bytes.resize(newHeaderSize);
    setLE(newHeaderSize - 4, checksum, 4);
    bytes.insert(bytes.end(), payloadSize, 0xa1);
    bytes.insert(bytes.end(), payloadSize, 0xb2);

    return bytes;
}

} // namespace pldm::fw_update::test
