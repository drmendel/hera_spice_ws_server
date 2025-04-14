#include <condition_variable>
#include <unordered_set>
#include <fstream>
#include <atomic>
#include <queue>

#include <uWebSockets/App.h>

#include <websocket_manager.hpp>
#include <spice_core.hpp>
#include <utils.hpp>

// Synchronization for message waiting
std::mutex spiceMutex;
std::condition_variable spiceCondition;
std::atomic<bool> spiceDataAvailable = false;

uint64_t ConnectionIDAllocator::allocate() {
    std::lock_guard<std::mutex> lock(mutex);

    uint64_t id;
    if (!availableIDs.empty()) { 
        // Reuse an old ID
        id = availableIDs.front();
        availableIDs.pop();
    } else {
        // Generate a new one
        id = nextID++;
    }

    activeIDs.insert(id);
    return id;
}

void ConnectionIDAllocator::release(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex);

    if (activeIDs.erase(id)) { 
        availableIDs.push(id);
    }
}

std::atomic<int> activeConnections = 0;
ConnectionIDAllocator idAllocator;
std::unordered_set<uWS::WebSocket<false, true, PerSocketData>*> activeSockets;
std::mutex appMutex;

// WebSocket event handlers
void onOpen(uWS::WebSocket<true, uWS::SERVER, PerSocketData> *ws) {
    PerSocketData* data = ws->getUserData();
    if (data) {
        data->id = idAllocator.allocate();
        activeConnections.fetch_add(1, std::memory_order_relaxed);

        std::cout << color("connect") << "Client connected with ID:    [" << data->id << "]\n";
        std::cout << color("reset") << "Active connections:          [" << activeConnections.load() << "]" << color("reset") << "\n\n";
    }
    {
        std::lock_guard<std::mutex> lock(appMutex);
        activeSockets.insert(reinterpret_cast<uWS::WebSocket<false, true, PerSocketData>*>(ws));
    }
}

void onMessage(uWS::WebSocket<true, uWS::SERVER, PerSocketData> *ws, std::string_view message, uWS::OpCode opCode) {

    // Invalid message: echo back & return
    if (message.length() != (sizeof(SpiceDouble) + sizeof(uint8_t) + sizeof(SpiceInt))) { 
        ws->send(message, opCode);
        return;
    }

    // Wait for spice data to be available
    std::unique_lock<std::mutex> lock(spiceMutex);
    spiceCondition.wait(lock, [] { return spiceDataAvailable.load(); });
    lock.unlock();

    // Process the request once data is available
    Request request(message);
    printResponse(request.getMessage());
    ws->send(request.getMessage(), opCode);
}

void onClose(uWS::WebSocket<true, uWS::SERVER, PerSocketData> *ws, int code, std::string_view message) {
    PerSocketData* data = ws->getUserData();
    if (data) {
        idAllocator.release(data->id);
        activeConnections.fetch_sub(1, std::memory_order_relaxed);
        std::cout << color("disconnect")<< "Client disconnected with ID: [" << data->id << "]\n";
        std::cout << color("reset") << "Active connections:          [" << activeConnections.load() << "]" << color("reset") << "\n\n";
    }
    {
        std::lock_guard<std::mutex> lock(appMutex);
        activeSockets.erase(reinterpret_cast<uWS::WebSocket<false, true, PerSocketData>*>(ws));
    }
}

uWS::SSLApp* app = nullptr;
uWS::Loop* loop = nullptr;
us_listen_socket_t* listenSocket = nullptr;

void shutdownServer() {
    std::unique_lock<std::mutex> lock(appMutex);
    
    std::cout << "\n" << color("reset") <<"Shutdown requested...\n";
    std::cout << "Closing " << activeSockets.size() << " connections..." << color("reset") << std::endl;
    us_listen_socket_close(0, listenSocket);    // stop recieind incoming connections
    auto socketsCopy = activeSockets;
    lock.unlock();

    for (auto* ws : socketsCopy) {
        if (ws) ws->end(1000, "Server shutdown");  // close all open connections
    }
}

const int serverOptions::getPort() const { return port; }
const int serverOptions::getInterval() const { return interval; }
const std::string serverOptions::getKey() const { return key; }
const std::string serverOptions::getCert() const { return cert; }
const std::string serverOptions::getPwd() const { return pwd; }

int serverOptions::load(int argc, char** argv) {
    if(argc != 6) {
        std::cerr << color("error") << "Error: Invalid number of arguments.\n\n" << color("reset");
        printUsage(argv);
        return 1;
    }

    try {
        this->port = std::stoi(argv[1]);
        this->interval = std::stoi(argv[2]);
    } catch (const std::invalid_argument& e) {
        std::cerr << color("error") << "Error: Port and interval must be valid integers.\n\n" << color("reset");
        printUsage(argv);
        return 1;
    } catch (const std::out_of_range& e) {
        std::cerr << color("error") << "Error: Port or interval value out of range.\n\n" << color("reset");
        printUsage(argv);
        return 1;
    }

    try {
        this->key = argv[3];
        this->cert = argv[4];

        std::ifstream pwdfile(argv[5]);
        if(pwdfile.is_open()) {
            std::getline(pwdfile, this->pwd);
            pwdfile.close();
        } else {
            std::cerr << color("error") << "Error: Passphrase file not available." << "\n\n";
            printUsage(argv);
            return 1;
        }
        
    } catch(const std::exception& e) {
        std::cerr << color("error") << "Error: " << e.what() << "\n\n";
        printUsage(argv);
        return 1;
    }



    return 0;
}