#pragma once

#include <config.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace pldm
{
namespace flightrecorder
{

using ReqOrResponse = bool;
using FlightRecorderData = std::vector<uint8_t>;
using FlightRecorderRecord = std::pair<ReqOrResponse, FlightRecorderData>;
using FlightRecorderCassette = std::vector<FlightRecorderRecord>;

/** @class FlightRecorder
 *
 *  The class for implementing the PLDM flight recorder logic. This class
 *  handles the insertion of the data into the recorder and also provides
 *  API's to dump the flight recorder into a file.
 */

class FlightRecorder
{
  private:
    FlightRecorder() : index(0)
    {
        tapeRecorder = FlightRecorderCassette(FLIGHT_RECORDER_MAX_SIZE);
    }

  protected:
    int index;
    FlightRecorderCassette tapeRecorder;

  public:
    FlightRecorder(const FlightRecorder&) = delete;
    FlightRecorder(FlightRecorder&&) = delete;
    FlightRecorder& operator=(const FlightRecorder&) = delete;
    FlightRecorder& operator=(FlightRecorder&&) = delete;
    ~FlightRecorder() = default;

    static FlightRecorder& GetInstance()
    {
        static FlightRecorder flightRecorder;
        return flightRecorder;
    }

    /** @brief Add records to the flightRecorder
     *
     *  @param[in] buffer  - The request/respose byte buffer
     *  @param[in] isRequest - bool that captures if it is a request message or
     *                         a response message
     *
     *  @return void
     */
    void saveRecord(const FlightRecorderData& buffer, ReqOrResponse isRequest)
    {
        tapeRecorder[index++ % FLIGHT_RECORDER_MAX_SIZE] =
            std::make_pair(isRequest, buffer);
    }

    /** @brief play flight recorder
     *
     *  @return void
     */

    void playRecorder()
    {
        std::ofstream recorderOutputFile(FLIGHT_RECORDER_DUMP_PATH);
        std::cout << "Dumping the flight recorder into : "
                  << FLIGHT_RECORDER_DUMP_PATH << "\n";

        for (const auto& message : tapeRecorder)
        {
            if (message.first)
            {
                recorderOutputFile << "Tx : \n";
            }
            else
            {
                recorderOutputFile << "Rx : \n";
            }
            for (const auto& word : message.second)
            {
                recorderOutputFile << std::setfill('0') << std::setw(2)
                                   << std::hex << (unsigned)word << " ";
            }
            recorderOutputFile << std::endl;
        }
        recorderOutputFile.close();
    }
};

} // namespace flightrecorder
} // namespace pldm
