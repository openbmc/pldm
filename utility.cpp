#include "utility.hpp"

namespace pldm
{
namespace utility
{

uint8_t getNumPadBytes(uint32_t data)
{
    uint8_t pad;
    pad = ((data % 4) ? (4 - data % 4) : 0);
    return pad;
} // end getNumPadBytes

bool uintToDate(uint64_t data, uint16_t* year, uint8_t* month, uint8_t* day,
                uint8_t* hour, uint8_t* min, uint8_t* sec)
{
    constexpr uint64_t max_data = 29991231115959;
    constexpr uint64_t min_data = 19700101000000;
    if (data < min_data || data > max_data)
    {
        return false;
    }

    *year = data / 10000000000;
    data = data % 10000000000;
    *month = data / 100000000;
    data = data % 100000000;
    *day = data / 1000000;
    data = data % 1000000;
    *hour = data / 10000;
    data = data % 10000;
    *min = data / 100;
    *sec = data % 100;

    return true;
}

bool decodeEffecterData(std::vector<uint8_t> effecterData,
                        uint16_t* effecter_id,
                        std::vector<set_effecter_state_field>* stateField)
{
    int flag = 0;
    for (auto data : effecterData)
    {
        switch (flag)
        {
            case 0:
                *effecter_id = data;
                flag = 1;
                break;
            case 1:
                if (data == PLDM_REQUEST_SET)
                {
                    flag = 2;
                }
                else
                {
                    stateField->push_back({PLDM_NO_CHANGE, 0});
                }
                break;
            case 2:
                stateField->push_back({PLDM_REQUEST_SET, data});
                flag = 1;
                break;
            default:
                break;
        }
    }

    if (stateField->size() < 1 || stateField->size() > 8)
    {
        return false;
    }

    return true;
}

} // namespace utility
} // namespace pldm
