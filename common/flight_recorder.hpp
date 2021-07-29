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

template <typename T>
class CircularBuffer
{
  public:
    CircularBuffer() : index(0)
    {
        cassette = std::vector<T>(FLIGHT_RECORDER_MAX_SIZE);
    }
    void addVoice(T voice)
    {
        cassette[index++ % FLIGHT_RECORDER_MAX_SIZE] = voice;
    }

    void dumpRecorder()
    {
        std::ofstream outputFile("/tmp/pldm_flight_recorder");
        std::cerr
            << "Dumping the flight recorder into /tmp/pldm_flight_recorder"
            << std::endl;

        for (const auto& message : cassette)
        {
            if (message.first)
            {
                outputFile << "Sent Message : \n";
            }
            else
            {
                outputFile << "Received Message : \n";
            }
            for (const auto& word : message.second)
            {
                outputFile << std::setfill('0') << std::setw(2) << std::hex
                           << (unsigned)word << " ";
            }
            outputFile << std::endl;
        }
    }

  private:
    int index;
    std::vector<T> cassette;
};

class FlightRecorder
{
  private:
    FlightRecorder()
    {}

  protected:
    CircularBuffer<std::pair<bool, std::vector<uint8_t>>> tapeRecorder;

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
    /** @brief Get the flight Recorder buffer
     *
     *  @return tapeRecorder - returns the circular buffer from memory
     */
    const CircularBuffer<std::pair<bool, std::vector<uint8_t>>>& getRecorder()
    {
        return tapeRecorder;
    }

    /** @brief Add records to the flightRecorder
     *
     *  @param[in] buffer  - The request/respose byte buffer
     *  @param[in] isRequest - bool that captures if it is a request message or
     *                         a response message
     *
     *  @return void
     */
    void saveRecord(const std::vector<uint8_t>& buffer, bool isRequest)
    {
        tapeRecorder.addVoice(std::make_pair(isRequest, buffer));
    }

    /** @brief play flight recorder
     *
     *  @return void
     */

    void playRecorder()
    {
        tapeRecorder.dumpRecorder();
    }
};

} // namespace flightrecorder
} // namespace pldm
