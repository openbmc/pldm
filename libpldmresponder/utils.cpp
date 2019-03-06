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

    seconds = std::stoi(sec);
    minutes = std::stoi(min);
    hours = std::stoi(hr);
    day = std::stoi(dy);
    year = std::stoi(yr);

    auto iter = months.find(mon);
    month = iter->second;
}

} // namespace responder
} // namespace pldm
