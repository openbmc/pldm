#include "utils.hpp"

#include <array>
#include <ctime>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace pldm
{

namespace responder
{

void formatTime(const std::string& s, uint8_t& seconds, uint8_t& minutes,
                uint8_t& hours, uint8_t& day, uint8_t& month, uint16_t& year)
{
    static const std::map<std::string, uint8_t> months{
        {"Jan", 1}, {"Feb", 2},  {"Mar", 3},  {"Apr", 4},
        {"May", 5}, {"Jun", 6},  {"Jul", 7},  {"Aug", 8},
        {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}};

    std::string sec = s.substr(17, 2);
    std::string min = s.substr(14, 2);
    std::string hr = s.substr(11, 2);
    std::string dy = s.substr(8, 2);
    std::string mon = s.substr(4, 3);
    std::string yr = s.substr(20, 4);

    decimalToBcd<uint8_t>(std::stoi(sec), seconds);
    decimalToBcd<uint8_t>(std::stoi(min), minutes);
    decimalToBcd<uint8_t>(std::stoi(hr), hours);
    decimalToBcd<uint8_t>(std::stoi(dy), day);
    decimalToBcd<uint16_t>(std::stoi(yr), year);

    auto iter = months.find(mon);
    decimalToBcd<uint8_t>(iter->second, month);
}

template <typename TYPE>
void decimalToBcd(TYPE decimal, TYPE& bcd)
{
    bcd = 0;
    TYPE rem = 0;
    auto cnt = 0;

    while (decimal)
    {
        rem = decimal % 10;
        bcd = bcd + (rem << cnt);
        decimal = decimal / 10;
        cnt += 4;
    }
}

} // namespace responder
} // namespace pldm
