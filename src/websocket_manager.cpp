#include <condition_variable>
#include <unordered_set>
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
std::mutex socketMutex;

// WebSocket event handlers
void onOpen(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws) {
    PerSocketData* data = ws->getUserData();
    if (data) {
        data->id = idAllocator.allocate();
        activeConnections.fetch_add(1, std::memory_order_relaxed);

        std::cout << color("connect") << "Client connected with ID:    [" << data->id << "]\n";
        std::cout << color("reset") << "Active connections:          [" << activeConnections.load() << "]" << color("reset") << "\n\n";
    }
    {
        std::lock_guard<std::mutex> lock(socketMutex);
        activeSockets.insert(reinterpret_cast<uWS::WebSocket<false, true, PerSocketData>*>(ws));
    }
}

void onMessage(uWS::WebSocket<false, true, PerSocketData> *ws, std::string_view message, uWS::OpCode opCode) {

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
    request.clearMessage();
    request.writeMessage();
    printResponse(request.getMessage());
    ws->send(request.getMessage(), opCode);
}

void onClose(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws, int code, std::string_view message) {
    PerSocketData* data = ws->getUserData();
    if (data) {
        idAllocator.release(data->id);
        activeConnections.fetch_sub(1, std::memory_order_relaxed);
        std::cout << color("disconnect")<< "Client disconnected with ID: [" << data->id << "]\n";
        std::cout << color("reset") << "Active connections:          [" << activeConnections.load() << "]" << color("reset") << "\n\n";
    }
    {
        std::lock_guard<std::mutex> lock(socketMutex);
        activeSockets.erase(reinterpret_cast<uWS::WebSocket<false, true, PerSocketData>*>(ws));
    }
}

uWS::App* app = nullptr;
uWS::Loop* loop = nullptr;
us_listen_socket_t* listenSocket = nullptr;

void shutdownServer() {
    std::unique_lock<std::mutex> lock(socketMutex);
    
    std::cout << "\n" << color("reset") <<"Shutdown requested...\n";
    std::cout << "Closing " << activeSockets.size() << " connections..." << color("reset") << std::endl;
    us_listen_socket_close(0, listenSocket);    // stop recieind incoming connections
    auto socketsCopy = activeSockets;
    lock.unlock();

    for (auto* ws : socketsCopy) {
        if (ws) ws->end(1000, "Server shutdown");  // close all open connections
    }
}