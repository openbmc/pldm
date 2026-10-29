#include "../regular_file_transfer.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

using namespace pldm::platform_mc::file_transfer;
using O = Section::Outcome;
static unsigned checks = 0;
static void require(bool value)
{
    ++checks;
    if (!value)
    {
        throw std::runtime_error("check " + std::to_string(checks));
    }
}
int main()
{
    require(negotiatePartSize(256, 512, true) == 256);
    require(!negotiatePartSize(256, 512, false));
    for (auto invalid : {0, 1, 255, 257, 511, 65535})
    {
        require(!negotiatePartSize(256, invalid, true));
    }
    require(validFileSize(0) == 0);
    require(validFileSize(UINT32_MAX) == UINT32_MAX);
    for (auto invalid : {-1., 0.5, double(UINT32_MAX) + 1,
                         std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity()})
    {
        require(!validFileSize(invalid));
    }
    require(readLength(1000, 100, 0) == 900);
    require(readLength(1000, 1000, 0) == 0);
    require(!readLength(1000, 1001, 0));
    require(!readLength(1000, 100, UINT64_MAX));
    require(!readLength(1000, UINT64_MAX, 1));
    std::vector<uint8_t> first(239, 0xa5), last(23, 0x5a);
    auto crc1 = pldm_edac_crc32(first.data(), first.size());
    auto combined = first;
    combined.insert(combined.end(), last.begin(), last.end());
    auto crc2 = pldm_edac_crc32(combined.data(), combined.size());
    Section s(262, 256);
    require(s.accept(8, 0, {}, 0) == O::invalid);       // early ACK
    require(s.accept(2, 1, first, crc1) == O::invalid); // early MIDDLE
    require(s.accept(1, 42, first, crc1 ^ 1) == O::retry);
    require(s.operation() == 4 && s.handle() == 0 && s.bytes().empty());
    require(s.accept(1, 42, first, crc1) == O::accepted);
    require(s.operation() == 1 && s.handle() == 42);
    require(s.accept(4, 0, last, crc2 ^ 1) == O::retry);
    require(s.handle() == 42 && s.bytes().size() == 239);
    require(s.accept(4, 0, last, crc2) == O::accepted);
    require(s.operation() == 3 && s.handle() == 0);
    require(s.accept(8, 0, last, 0) == O::invalid);
    require(s.accept(8, 0, {}, 0) == O::complete);
    require(s.bytes().size() == 262);
    require(s.accept(8, 0, {}, 0) == O::invalid); // duplicate terminal
    Section bad(239, 256);
    for (int i = 0; i < 3; ++i)
    {
        require(bad.accept(5, 0, first, crc1 ^ 1) == O::retry);
    }
    require(bad.accept(5, 0, first, crc1 ^ 1) == O::invalid);
    Section shortFile(262, 256);
    require(shortFile.accept(5, 0, last,
                             pldm_edac_crc32(last.data(), last.size())) ==
            O::invalid);
    Section oversized(70000, 256);
    require(oversized.accept(1, 1, first, crc1) == O::invalid);
    // Multi-part section across every supported part boundary.
    for (uint16_t partSize : {256, 512, 1024, 2048})
    {
        Section section(65536, partSize);
        std::vector<uint8_t> all(65536);
        uint32_t crc = 0;
        size_t offset = 0;
        while (offset < all.size())
        {
            auto count = std::min<size_t>(partSize - 17, all.size() - offset);
            std::span<const uint8_t> part(all.data() + offset, count);
            crc = pldm_edac_crc32(all.data(), offset + count);
            bool end = offset + count == all.size();
            uint8_t flag = offset == 0 ? (end ? 5 : 1) : (end ? 4 : 2);
            require(section.accept(flag, end ? 0 : offset + 123, part, crc) ==
                    O::accepted);
            offset += count;
        }
        require(section.accept(8, 0, {}, 0) == O::complete);
        require(std::equal(section.bytes().begin(), section.bytes().end(),
                           all.begin()));
    }
    std::cout << checks << " regular transfer checks passed\n";
}
