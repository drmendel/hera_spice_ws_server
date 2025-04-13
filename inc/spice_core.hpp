#ifndef SPICE_CORE_HPP
#define SPICE_CORE_HPP

#include <string>
#include <cstdint>
#include <unordered_map>

#include <uWebSockets/App.h>
#include <cspice/SpiceUsr.h>

#include <data_manager.hpp>

const std::unordered_map<int32_t, std::string_view> objects = {
    {10, "SUN"},
        {199, "MERCURY"},
        {299, "VENUS"},
        {399, "EARTH"},
            {301, "MOON"},
        {499, "MARS"},
            {401, "PHOBOS"},
            {402, "DEIMOS"},
        
    {-658030, "DIDYMOS"},
        {-658031, "DIMORPHOS"},             // no data available yet
            {-91900, "DART_IMPACT_SITE"},       // no data available yet, special calculation

    {-91000, "HERA_SPACECRAFT"},
        {-15513000, "JUVENTAS_SPACECRAFT"},
        {-9102000, "MILANI_SPACECRAFT"}      // no such id
};  // 8 solorsystem body + 3 asteroid + 3 spacecraft = 14 objects

/*
const std::unordered_map<int32_t, std::string_view> heraCameras = {
    {-91500, "HERA_SMC"},
    {-91400, "HERA_HSH"},
    {-91200, "HERA_TIRI"},
    {-91120, "HERA_AFC-2"},
    {-91110, "HERA_AFC-1"}
};*/

struct Vector {
    SpiceDouble x, y, z;
};
struct Quaternion {
    SpiceDouble w, x, y, z;
};
struct MotionState {
    Vector position;
    Vector velocity;
    Quaternion orientation;
    Vector angularVelocity;
};

class ObjectData {
public:
    ObjectData(SpiceDouble et, SpiceInt objectId, SpiceInt observerId, bool lightTimeAdjusted = false);
    void serializeToBinary(std::string& buffer) const;
private:
    SpiceDouble et;
    SpiceInt objectId;
    SpiceInt observerId;
    SpiceBoolean lightTimeAdjusted;

    MotionState objectState;
    SpiceBoolean stateAvailable;
    bool loadState();
};

enum class MessageMode : uint8_t {
    ALL_INSTANTANEOUS = 'i',
    ALL_LIGHT_TIME_ADJUSTED = 'l',
    ERROR = 'e'
};

class Request {
public:
    Request(std::string_view incomingRequest);              // Loads the request values into the class
    void clearMessage();                            // Cleares the message buffer
    int writeMessage();                             // 0 succes, 1 writeHeader() failed, 2 writeData() failed, 3 - both failed
    std::string getMessage() const;                 // Return the binary response string
private:
    SpiceDouble utcTimestamp;                          // Request data
    MessageMode mode;                               // Request data
    SpiceInt observerId;                             // Request data

    SpiceDouble et;

    std::string_view request;                       // Incoming request
    std::string message;                            // Outgoing message

    void setETime(SpiceDouble utcTimestamp);
    void setMode(MessageMode mode);
    void setObserverId(SpiceInt observerId);
    
    int writeHeader();                              // Writes the header to the message buffer: 0 success, 1 failiure
    int writeData(SpiceBoolean lightTimeAdjusted);  // Writes the data to the message buffer: 0 success, 1 failiure
};

void initSpiceCore();
void deinitSpiceCore();
std::string getBodyFixedFrameName(SpiceInt id);

std::string utcTime(SpiceDouble utcTimestamp);
SpiceDouble etTime(SpiceDouble utcTimestamp);

std::string getName(SpiceInt id);

std::string createMessage(SpiceDouble utcTime, uint8_t mode, SpiceInt id);
void printRequest(const std::string& binaryData);
void printResponse(const std::string& binaryData);
void printHex(const std::string& binaryData);

#endif // SPICE_CORE_HPP