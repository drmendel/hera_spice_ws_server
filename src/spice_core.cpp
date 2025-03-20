//#include <iostream>
#include <cmath>

#include <spice_core.hpp>

// OBJECTDATA
ObjectData::ObjectData(SpiceDouble et, SpiceDouble objectId, SpiceInt observerId, bool lightTimeAdjusted) {
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
    buffer.append(reinterpret_cast<const char*>(&objectState), sizeof(objectState));
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

    // Load position and velocity
    SpiceDouble spiceState[6], lt;
    spkez_c(objectId, et, "J2000", lightTimeAdjusted ? "LT+S" : "NONE", observerId, spiceState, &lt);
    if (failed_c()) {
        reset_c();
        return (stateAvailable = false);
    }

    objectState.position = {spiceState[0], spiceState[1], spiceState[2]};
    objectState.velocity = {spiceState[3], spiceState[4], spiceState[5]};

    // Orientation and angular velocity
    SpiceDouble xform[6][6];
    SpiceDouble correctedET = lightTimeAdjusted ? et - lt : et;
    sxform_c("J2000", bodyFixedFrame.c_str(), correctedET, xform);
    if (failed_c()) {
        reset_c();
        return (stateAvailable = false);
    }

    SpiceDouble rotationMatrix[3][3], quaternion[4], angularVelocity[3];
    xf2rav_c(xform, rotationMatrix, angularVelocity);
    m2q_c(rotationMatrix, quaternion);

    objectState.orientation = {quaternion[0], quaternion[1], quaternion[2], quaternion[3]};
    objectState.angularVelocity = {angularVelocity[0], angularVelocity[1], angularVelocity[2]};

    return (stateAvailable = true);
}






















Request::Request(std::string_view incomingRequest) : request(incomingRequest) {    
    std::memcpy(&utcTimestamp, request.data(), sizeof(uint32_t)); // Read first 4 bytes
    this->mode = static_cast<MessageMode>(request[sizeof(uint32_t)]); // Read byte at index 4
    std::memcpy(&observerId, request.data() + sizeof(uint32_t) + sizeof(uint8_t), sizeof(int32_t)); // Read 4 bytes from index 5
    this->setETime(utcTimestamp);
    this->writeMessage();
}
std::string Request::getMessage() const {
    return message;
}



void Request::setETime(uint32_t utcTimestamp) {
    this->et = etTime(utcTimestamp);
}
void Request::setMode(MessageMode mode) {
    this->mode = mode;
}
void Request::setObserverId(int32_t observerId) {
    this->observerId = observerId;
}



void Request::clearMessage() {
    this->message.clear();
}
int Request::writeMessage() {
    int error = 0;
    error |= writeHeader();
    error |= writeData(mode == MessageMode::ALL_LIGHT_TIME_ADJUSTED);
    if(!error) return 0;
    
    message[5] = (uint8_t)MessageMode::ERROR;
    return -1;
}



int Request::writeHeader() {
    int size = message.size();
    message.append(reinterpret_cast<const char*>(&utcTimestamp), sizeof(utcTimestamp));
    message.append(reinterpret_cast<const char*>(&mode), sizeof(mode));
    if((message.size() - size) != 5) return 1;
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
    furnsh_c(cremaMetakernel.c_str());
    //furnsh_c(planMetakernel.c_str());
    //furnsh_c(studyMetakernel.c_str());
    furnsh_c(operationalMetakernel.c_str());
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





std::string utcTime(uint32_t utcTimestamp) {
    uint32_t seconds = utcTimestamp % 60;
    uint32_t minutes = (utcTimestamp / 60) % 60;
    uint32_t hours = (utcTimestamp / 3600) % 24;
    uint32_t days = utcTimestamp / 86400;

    // Calculate date starting from 1970-01-01
    uint32_t year = 1970;
    uint32_t month = 1;
    uint32_t day = 1;

    static const uint8_t daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    while (days >= 365) {
        bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        uint32_t yearDays = leap ? 366 : 365;
        if (days >= yearDays) {
            days -= yearDays;
            year++;
        } else {
            break;
        }
    }

    while (days >= (daysInMonth[month - 1] + (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))))) {
        days -= daysInMonth[month - 1] + (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)));
        month++;
    }

    day += days;

    // Format as ISO 8601
    std::ostringstream utcStream;
    utcStream << year << '-'
              << std::setw(2) << std::setfill('0') << month << '-'
              << std::setw(2) << std::setfill('0') << day << 'T'
              << std::setw(2) << std::setfill('0') << hours << ':'
              << std::setw(2) << std::setfill('0') << minutes << ':'
              << std::setw(2) << std::setfill('0') << seconds;
    
    return utcStream.str();
}

SpiceDouble etTime(uint32_t utcTimestamp) {
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





std::string createMessage(uint32_t utcTime, uint8_t mode, int32_t id) {
    std::string message;
    message.resize(sizeof(uint32_t) + sizeof(uint8_t) + sizeof(int32_t));

    std::memcpy(&message[0], &utcTime, sizeof(uint32_t));  // Store UTC time
    std::memcpy(&message[sizeof(uint32_t)], &mode, sizeof(uint8_t));  // Store mode
    std::memcpy(&message[sizeof(uint32_t) + sizeof(uint8_t)], &id, sizeof(int32_t));  // Store ID

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
        std::cout << "-----------------------------------------\n\n";
        printInt(); // int32

        printDouble(3);
        printDouble(3);
        printDouble(4);
        printDouble(3);
    }
    std::cout << "-----------------------------------------\n";
}

static void printHeader(const std::string& binaryData) {
    if (binaryData.size() < sizeof(uint32_t) + sizeof(uint8_t)) {
        std::cerr << "Error: Binary data too small!\n";
        return;
    }

    // Extract uint32_t (UTC time)
    uint32_t utcTime;
    std::memcpy(&utcTime, binaryData.data(), sizeof(uint32_t));

    // Extract uint8_t (mode/status)
    uint8_t mode;
    std::memcpy(&mode, binaryData.data() + sizeof(uint32_t), sizeof(uint8_t));

    std::string line(41, '-'); // Box width
    std::cout << "\n" << line << "\n";
    std::cout << "|          " << std::setw(28) << std::left << "Header Information" << " |\n" << std::right;
    std::cout << line << "\n";
    std::cout << "| UTC Time: " << " 0x "
              << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << utcTime
              << " ( " << std::dec << std::setw(10) << utcTime << " ) |\n";
    std::cout << "| Mode:     " << " 0x       "
              << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << static_cast<int>(mode)
              << " ( " << std::dec << std::setw(10) << std::setfill(' ') << static_cast<int>(mode) << " ) |\n";
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
    constexpr size_t headerSize = sizeof(uint32_t) + sizeof(uint8_t);
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