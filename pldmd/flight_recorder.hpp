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
    void add_voice(T voice)
    {
        cassette[index++ % FLIGHT_RECORDER_MAX_SIZE] = voice;
    }

    void dump_recorder()
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
  protected:
    static FlightRecorder* flightRecorder;
    CircularBuffer<std::pair<bool, std::vector<uint8_t>>> tapeRecorder;

  public:
    void operator=(const FlightRecorder&) = delete;

    static FlightRecorder* GetInstance()
    {
        if (flightRecorder == nullptr)
        {
            flightRecorder = new FlightRecorder;
        }
        return flightRecorder;
    }

    const CircularBuffer<std::pair<bool, std::vector<uint8_t>>>& getRecorder()
    {
        return tapeRecorder;
    }

    void saveRecord(const std::vector<uint8_t>& buffer, bool isRequest)
    {
        tapeRecorder.add_voice(std::make_pair(isRequest, buffer));
    }

    void playRecorder()
    {
        tapeRecorder.dump_recorder();
    }
};

} // namespace flightrecorder
} // namespace pldm
