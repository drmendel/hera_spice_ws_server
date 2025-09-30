/*
 *  Copyright 2025 Mendel Dobondi-Reisz
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef SPICE_CORE_HPP
#define SPICE_CORE_HPP

// Standard C++ Libraries
#include <unordered_map>
#include <filesystem>
#include <cstdint>
#include <string>

// External Libraries
#include <cspice/SpiceUsr.h>

// ─────────────────────────────────────────────
// Objects - all relevant objects in the mission
// ─────────────────────────────────────────────
const std::unordered_map<SpiceInt, std::string_view> objects = {
    {0, "SOLAR_SYSTEM_BARYCENTER"},
    {10, "SUN"},
    {199, "MERCURY"},
    {299, "VENUS"},
    {399, "EARTH"},
    {301, "MOON"},
    {499, "MARS"},
    {401, "PHOBOS"},
    {402, "DEIMOS"},
    {-658030, "DIDYMOS"},
    {-658031, "DIMORPHOS"},
    {-91900, "DART_IMPACT_SITE"},
    {-91000, "HERA_SPACECRAFT"},
    {-9101000, "JUVENTAS_SPACECRAFT"},
    {-9102000, "MILANI_SPACECRAFT"}
};

// ─────────────────────────────────────────────
// Object Motion State Data
// ─────────────────────────────────────────────
struct Vector { SpiceDouble x, y, z; };
struct Quaternion { SpiceDouble x, y, z, w; };
struct MotionState {
    Vector position;
    Vector velocity;
    Quaternion orientation;
    Vector angularVelocity;
};

// ─────────────────────────────────────────────
// Object Data - motion snapshots for objects
// ─────────────────────────────────────────────
class ObjectData {
public:
    ObjectData(SpiceDouble et, SpiceInt objectId, SpiceInt observerId, bool lightTimeAdjusted);
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

// ─────────────────────────────────────────────
// Message Modes / Error Codes
// ─────────────────────────────────────────────
enum class MessageMode : uint8_t {
    ALL_INSTANTANEOUS = 'i',
    ALL_LIGHT_TIME_ADJUSTED = 'l',
    ERROR = 'e',
    ERROR_I = 'f',
    ERROR_L = 'g'
};

// ─────────────────────────────────────────────
// Request - processing incoming requests
// ─────────────────────────────────────────────
class RequestHandler {
private:
    // SPICE ephemeris time
    SpiceDouble et;
    
    // Request data
    SpiceDouble utcTimestamp;
    MessageMode mode;
    SpiceInt observerId;

    // Request, response containers
    std::string_view request;
    std::string message;

    // Request setters
    void setETime(SpiceDouble utcTimestamp);
    void setMode(MessageMode mode);
    void setObserverId(SpiceInt observerId);
    
    // Message writers
    int writeHeader();                              // Writes the header to the message buffer: 0 success, 1 failiure
    int writeData(SpiceBoolean lightTimeAdjusted);  // Writes the data to the message buffer: 0 success, 1 failiure

public:
    RequestHandler(std::string_view incomingRequest);
    
    // Message modifiers
    void clearMessage();
    int writeMessage();

    // Getter
    std::string getMessage() const;
};

// ─────────────────────────────────────────────
// SPICE Core Management
// ─────────────────────────────────────────────
extern bool kernelPathsLoaded;
extern std::filesystem::path cremaMetakernel;
extern std::filesystem::path operationalMetakernel;
extern std::filesystem::path planMetakernel;
void initSpiceCore();
void deinitSpiceCore();
SpiceDouble etTime(SpiceDouble utcTimestamp);
std::string getBodyFixedFrameName(SpiceInt id);
std::string utcTimeString(SpiceDouble utcTimestamp);
std::string getName(SpiceInt id);

// ─────────────────────────────────────────────
// Printer Functions
// ─────────────────────────────────────────────
void printResponse(const std::string& binaryData);
void printData(const std::string& binaryData);
void printHeader(const std::string& binaryData);

#endif // SPICE_CORE_HPP
