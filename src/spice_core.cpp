//#include <iostream>
#include <cmath>

#include <cspice/SpiceUsr.h>
#include <spice_core.hpp>

// OBJECTDATA
ObjectData::ObjectData(SpiceDouble et, SpiceInt objectId, SpiceInt observerId, bool lightTimeAdjusted) {
    this->et = et;
    this->objectId = objectId;
    this->observerId = observerId;
    this->lightTimeAdjusted = lightTimeAdjusted;
    this->stateAvailable = loadState();
}

void ObjectData::serializeToBinary(std::string& buffer) const {
    if (!stateAvailable) return;

    buffer.reserve(buffer.size() + sizeof(objectId) + sizeof(objectState));

    buffer.append(reinterpret_cast<const char*>(&objectId), sizeof(objectId));
    buffer.append(reinterpret_cast<const char*>(&objectState.position), sizeof(objectState.position));
    buffer.append(reinterpret_cast<const char*>(&objectState.velocity), sizeof(objectState.velocity));
    buffer.append(reinterpret_cast<const char*>(&objectState.orientation), sizeof(objectState.orientation));
    buffer.append(reinterpret_cast<const char*>(&objectState.angularVelocity), sizeof(objectState.angularVelocity));
}


bool ObjectData::loadState() {
    
    // Find object’s body-fixed frame
    std::string bodyFixedFrame = getBodyFixedFrameName(objectId);
    if (bodyFixedFrame == "UNKNOWN") {
        std::cerr << "No valid frame found for ID: " << objectId << std::endl;
        stateAvailable = false;
        return false;
    }

    // Set error handling to return on error
    erract_c("SET", 6, (SpiceChar*)"RETURN");

    #ifdef DOCKER
        errdev_c("SET", 0, (SpiceChar *)"NULL");
    #endif

    // Load position and velocity
    SpiceDouble spiceState[6], lt;

    SpiceBoolean found = false;
    SpiceChar objectName[32];
    SpiceChar observerName[32];
    bodc2n_c(objectId, 32, objectName, &found);
    bodc2n_c(observerId, 32, observerName, &found);

    if(objectId == -9102000)        spkezr_c("MILANI", et, "J2000", lightTimeAdjusted ? (objectId == observerId ? "NONE" : "LT") : "NONE", observerName, spiceState, &lt);
    else if(observerId == -9102000) spkezr_c(objectName, et, "J2000", lightTimeAdjusted ? (objectId == observerId ? "NONE" : "LT") : "NONE", "MILANI", spiceState, &lt);
    else                            spkez_c(objectId, et, "J2000", lightTimeAdjusted ? (objectId == observerId ? "NONE" : "LT") : "NONE", observerId, spiceState, &lt);
    if (failed_c()) {
        reset_c();
        return (stateAvailable = false);
    }

    objectState.position = {spiceState[0], spiceState[1], spiceState[2]};
    objectState.velocity = {spiceState[3], spiceState[4], spiceState[5]};

    // Orientation and angular velocity
    SpiceDouble xform[6][6];
    SpiceDouble correctedET = lightTimeAdjusted ? et - lt : et;
    if(objectId == -9102000) sxform_c("MILANI_SPACECRAFT", "J2000", correctedET, xform);
    else sxform_c(bodyFixedFrame.c_str(), "J2000", correctedET, xform);

    if (failed_c()) {
        reset_c();
        return (stateAvailable = false);
    }

    SpiceDouble rotationMatrix[3][3], quaternion[4], angularVelocity[3];
    xf2rav_c(xform, rotationMatrix, angularVelocity);
    m2q_c(rotationMatrix, quaternion);

    /* 
     * Spice quaternion order: w, x, y, z
     * Three.js quaternion order: x, y, z, w
     */
    objectState.orientation = {quaternion[1], quaternion[2], quaternion[3], quaternion[0]};
    objectState.angularVelocity = {angularVelocity[0], angularVelocity[1], angularVelocity[2]};

    return (stateAvailable = true);
}






















Request::Request(std::string_view incomingRequest) : request(incomingRequest) {    
    std::memcpy(&utcTimestamp, request.data(), sizeof(utcTimestamp)); // Read first 8 bytes (double)
    this->mode = static_cast<MessageMode>(request[sizeof(utcTimestamp)]); // Read byte at index 8
    std::memcpy(&observerId, request.data() + sizeof(utcTimestamp) + sizeof(mode), sizeof(observerId));
    this->setETime(utcTimestamp);
    this->clearMessage();
    this->writeMessage();
}
std::string Request::getMessage() const {
    return message;
}



void Request::setETime(SpiceDouble utcTimestamp) {
    this->et = etTime(utcTimestamp);
}
void Request::setMode(MessageMode mode) {
    this->mode = mode;
}
void Request::setObserverId(SpiceInt observerId) {
    this->observerId = observerId;
}



void Request::clearMessage() {
    this->message.clear();
}
int Request::writeMessage() {
    int error = 0;
    error |= writeHeader();
    if(this->mode != MessageMode::ALL_INSTANTANEOUS && this->mode != MessageMode::ALL_LIGHT_TIME_ADJUSTED) {
        message[8] = (uint8_t)MessageMode::ERROR;
        error |= true;
        return -1;
    }
    error |= writeData(mode == MessageMode::ALL_LIGHT_TIME_ADJUSTED);
    if(!error) return 0;

    message[8] = (mode == MessageMode::ALL_LIGHT_TIME_ADJUSTED) ? (uint8_t)MessageMode::ERROR_L : (uint8_t)MessageMode::ERROR_I;

    return -1;
}



int Request::writeHeader() {
    int size = message.size();
    message.append(reinterpret_cast<const char*>(&utcTimestamp), sizeof(utcTimestamp));
    message.append(reinterpret_cast<const char*>(&mode), sizeof(mode));
    if((message.size() - size) != 9) return 1;
    return 0;
}
int Request::writeData(SpiceBoolean lightTimeAdjusted) {
    int size = message.size();
    for (const auto& [objectId, name] : objects) {
        ObjectData obj(et, objectId, observerId, lightTimeAdjusted);
        obj.serializeToBinary(message);
    }
    if((message.size() - size) <= 0) return 1;
    return 0;
}




















void initSpiceCore() {
    std::filesystem::path operationalMetakernel = getExecutablePath().parent_path().parent_path() / "data" / "hera" / "kernels" / "mk" / "hera_ops.tm";
    std::filesystem::path planMetakernel = getExecutablePath().parent_path().parent_path() / "data" / "hera" / "kernels" / "mk" / "hera_plan.tm";
    std::filesystem::path studyMetakernel = getExecutablePath().parent_path().parent_path() / "data" / "hera" / "kernels" / "mk" / "hera_study_PO_EMA_2024.tm";
    std::filesystem::path cremaMetakernel = getExecutablePath().parent_path().parent_path() / "data" / "hera" / "kernels" / "mk" / "hera_crema_2_1.tm";

    // furnsh_c(studyMetakernel.c_str());

    furnsh_c(cremaMetakernel.c_str());          // 2024-10-08   2027-03-02      movement is ok, no juventas, milani, no mars flyby
    furnsh_c(operationalMetakernel.c_str());    // 2024-10-08   2026-10-27      accurate
    furnsh_c(planMetakernel.c_str());           // 2024-10-08   2027-04-16      movement is ok, juventas, milani, too (similarity to ops)
}

void deinitSpiceCore() {
    kclear_c();
}

std::string getBodyFixedFrameName(SpiceInt id) {
    SpiceInt frameCode;
    SpiceChar frameName[32] = {0};
    SpiceBoolean found = false;

    // Try to get a body-fixed frame
    cidfrm_c(id, sizeof(frameName), &frameCode, frameName, &found);
    if (found) return frameName;

    // Try to get a named frame (works for spacecraft/cameras)
    frmnam_c(id, sizeof(frameName), frameName);
    return (frameName[0] != '\0') ? frameName : "UNKNOWN";
}




std::string utcTime(SpiceDouble utcTimestamp) {
    // Separate seconds and fractional part
    uint32_t seconds = static_cast<uint32_t>(utcTimestamp);
    double fractionalSeconds = utcTimestamp - seconds;

    // Convert total seconds into hours, minutes, and seconds
    uint32_t minutes = (seconds / 60) % 60;
    uint32_t hours = (seconds / 3600) % 24;
    uint32_t totalDays = seconds / 86400;  // Total number of days since 1970-01-01

    // Calculate date starting from 1970-01-01
    uint32_t year = 1970;
    uint32_t month = 1;
    uint32_t day = 1;

    // Days in each month (non-leap years)
    static const uint8_t daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    // Handle the passage of days over multiple years
    while (totalDays >= 365) {
        bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));  // Leap year check
        uint32_t yearDays = leap ? 366 : 365;

        if (totalDays >= yearDays) {
            totalDays -= yearDays;
            year++;
        } else {
            break;
        }
    }

    // Handle months and days within the current year
    while (totalDays >= daysInMonth[month - 1] + (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))) {
        totalDays -= daysInMonth[month - 1] + (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)));
        month++;
    }

    day += totalDays;

    // Format as ISO 8601 with fractional seconds
    std::ostringstream utcStream;
    utcStream << year << '-'
              << std::setw(2) << std::setfill('0') << month << '-'
              << std::setw(2) << std::setfill('0') << day << 'T'
              << std::setw(2) << std::setfill('0') << hours << ':'
              << std::setw(2) << std::setfill('0') << minutes << ':'
              << std::setw(2) << std::setfill('0') << (seconds % 60);  // Use seconds mod 60 for accuracy

    if (fractionalSeconds > 0) {
        uint32_t microseconds = static_cast<uint32_t>(fractionalSeconds * 1e6);  // Convert fractional seconds to microseconds
        utcStream << '.' << std::setw(6) << std::setfill('0') << microseconds;  // Add fractional part
    }

    return utcStream.str();
}


SpiceDouble etTime(SpiceDouble utcTimestamp) {
    SpiceDouble et;
    str2et_c(utcTime(utcTimestamp).c_str(), &et);
    return et;
}





std::string getName(SpiceInt id) {
    char name[32];
    SpiceBoolean found;
    bodc2n_c(id, 32, name, &found);
    return found ? std::string(name) : "UNKNOWN_CAMERA";
}





std::string createMessage(SpiceDouble utcTime, uint8_t mode, SpiceInt id) {
    std::string message;
    message.resize(sizeof(utcTime) + sizeof(mode) + sizeof(id));

    std::memcpy(&message[0], &utcTime, sizeof(utcTime));  // Store UTC time
    std::memcpy(&message[sizeof(utcTime)], &mode, sizeof(mode));  // Store mode
    std::memcpy(&message[sizeof(utcTime) + sizeof(mode)], &id, sizeof(id));  // Store ID

    return message;
}

static void printData(const std::string& binaryData) {
    size_t index = 0;
    const size_t structSize = 4 + (3 + 3 + 4 + 3) * 8; // Total bytes per struct

    auto printInt = [&]() {
        if (index + 4 > binaryData.size()) return;
        int32_t value = *reinterpret_cast<const int32_t*>(&binaryData[index]);

        std::cout << std::setw(12) << value << "   ";
        for (size_t i = 0; i < 4; ++i, ++index) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(binaryData[index])) << " ";
        }
        std::cout << std::dec << std::setfill(' ') << "\n\n";
    };

    auto printDouble = [&](size_t count) {
        for (size_t i = 0; i < count; ++i) {
            if (index + 8 > binaryData.size()) return;
            double value = *reinterpret_cast<const double*>(&binaryData[index]);

            std::cout << std::setw(12) << std::scientific << std::setprecision(4) << std::uppercase 
                      << std::right << value << "   ";
            for (size_t j = 0; j < 8; ++j, ++index) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(static_cast<unsigned char>(binaryData[index])) << " ";
                if ((j + 1) % 4 == 0) std::cout << " ";
            }
            std::cout << std::dec << std::setfill(' ') << "\n";
        }
        std::cout << "\n";
    };

    while (index + structSize <= binaryData.size()) {
        
        printInt(); // int32

        printDouble(3);
        printDouble(3);
        printDouble(4);
        printDouble(3);
        std::cout << "-----------------------------------------\n\n";
    }
}

static void printHeader(const std::string& binaryData) {
    if (binaryData.size() < sizeof(SpiceDouble) + sizeof(uint8_t)) {
        std::cerr << "Error: Binary data too small!\n";
        return;
    }

    // Extract SpiceDouble (UTC time)
    SpiceDouble utcTime;
    std::memcpy(&utcTime, binaryData.data(), sizeof(utcTime));

    // Extract uint8_t (mode/status)
    uint8_t mode;
    std::memcpy(&mode, binaryData.data() + sizeof(SpiceDouble), sizeof(uint8_t));

    int index = 0;
    auto printDouble = [&]() {
        if (index + 8 > binaryData.size()) return;

        for (size_t i = 0; i < 8; ++i, ++index) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(binaryData[index])) << " ";
        }
        std::cout << std::dec << std::setfill(' ');
    };

    std::string line(60, '-'); // Box width
    std::cout << "\n" << line << "\n";
    std::cout << "|                 " << std::setw(40) << std::left << "Header Information" << " |\n" << std::right;
    std::cout << line << "\n";
    std::cout << "| UTC Time: " << "0x ";
    printDouble();
              // << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << static_cast<uint32_t>(utcTime)
    std::cout << "      (" << std::setw(10) << static_cast<SpiceDouble>(utcTime) << ") |\n";
    std::cout << "| Mode:    " << " 0x "
              << std::setw(2) << std::left << std::setfill('0') << std::hex << std::uppercase << static_cast<int>(mode) << std::setfill(' ')
              << std::setw(29) << std::right << " ("  << std::dec  << std::setw(11) << std::setfill(' ') << static_cast<int>(mode) << ") |\n";
    std::cout << line << "\n";
}

void printRequest(const std::string& binaryData) {
    size_t index = 0;

    auto printInt = [&]() {
        if (index + 4 > binaryData.size()) return;
        int32_t value = *reinterpret_cast<const int32_t*>(&binaryData[index]);

        std::cout << std::setw(12) << value << "   ";
        for (size_t i = 0; i < 4; ++i, ++index) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(binaryData[index])) << " ";
        }
        std::cout << std::dec << std::setfill(' ') << "\n\n";
    };

    uint8_t md = binaryData[5];

    while (index <= binaryData.size()) {
        std::cout << "-----------------------------------------\n\n";
        printInt(); // int32
        std::cout << std::setw(11) << (int)md << "   " << std::hex << std::setw(2) << std::setfill('0'); index++;  // uint8
        std::cout << std::dec << std::setfill(' ') << "\n";
        printInt();
    }
}

void printResponse(const std::string& binaryData) {
    constexpr size_t headerSize = sizeof(SpiceDouble) + sizeof(uint8_t);
    if (binaryData.size() < headerSize) {
        std::cerr << "Error: Binary data too small!\n";
        return;
    }
    printHeader(binaryData);
    std::string dataPart = binaryData.substr(headerSize);
    printData(dataPart);
}

void printHex(const std::string& binaryData) {
    std::cout << "Little-endian binary message (hex): \n";
    for (unsigned char c : binaryData) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << std::setfill(' ') << std::endl; // Reset to decimal format
}
