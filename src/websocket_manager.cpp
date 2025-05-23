// C++ Standard Libraries
#include <condition_variable>
#include <unordered_set>
#include <string_view>
#include <iostream>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <queue>

// External Libraries
#include <uWebSockets/App.h>

// Project Headers
#include <websocket_manager.hpp>
#include <spice_core.hpp>
#include <utils.hpp>



// ─────────────────────────────────────────────
// Synchronization for Message Waiting
// ─────────────────────────────────────────────

std::mutex spiceMutex;
std::condition_variable spiceCondition;
std::atomic<bool> spiceDataAvailable = false;



// ─────────────────────────────────────────────
// Connection ID Allocator
// ─────────────────────────────────────────────

uint64_t ConnectionIDAllocator::allocate() {
    std::lock_guard<std::mutex> lock(mutex);

    uint64_t id;
    if (!availableIDs.empty()) { 
        id = availableIDs.front();
        availableIDs.pop();
    }
    else id = nextID++;

    activeIDs.insert(id);
    return id;
}

void ConnectionIDAllocator::release(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex);
    if (activeIDs.erase(id)) availableIDs.push(id);
}

std::atomic<int> activeConnections = 0;
ConnectionIDAllocator idAllocator;



// ─────────────────────────────────────────────
// WebSocket Connection Data
// ─────────────────────────────────────────────

std::mutex socketMutex;
std::unordered_set<uWS::WebSocket<false, true, PerSocketData>*> activeSockets;



// ─────────────────────────────────────────────
// WebSocket Event Handlers
// ─────────────────────────────────────────────

void onOpen(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws) {
    PerSocketData* data = ws->getUserData();
    if (data) {
        data->id = idAllocator.allocate();
        activeConnections.fetch_add(1, std::memory_order_relaxed);

        std::cout << color("connect") << "Client connected with ID:    [" << data->id << "]\n";
        std::cout << color("log") << "Active connections:          [" << activeConnections.load() << "]" << "\n\n" << std::flush;
    }
    
    {
        std::lock_guard<std::mutex> lock(socketMutex);
        activeSockets.insert(reinterpret_cast<uWS::WebSocket<false, true, PerSocketData>*>(ws));
    }
}

void onMessage(uWS::WebSocket<false, true, PerSocketData> *ws, std::string_view message, uWS::OpCode opCode) {
    if (message.length() != (sizeof(SpiceDouble) + sizeof(uint8_t) + sizeof(SpiceInt))) { 
        ws->send(message, opCode);
        return;
    }

    std::unique_lock<std::mutex> lock(spiceMutex);
    spiceCondition.wait(lock, [] { return spiceDataAvailable.load(); });
    lock.unlock();

    Request request(message);
    ws->send(request.getMessage(), opCode);

    #ifndef DEBUG
        printResponse(request.getMessage());
    #endif
}

void onClose(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws, int code, std::string_view message) {
    PerSocketData* data = ws->getUserData();
    if (data) {
        idAllocator.release(data->id);
        activeConnections.fetch_sub(1, std::memory_order_relaxed);
        std::cout << color("disconnect")<< "Client disconnected with ID: [" << data->id << "]\n";
        std::cout << color("log") << "Active connections:          [" << activeConnections.load() << "]" << "\n\n";
    }
    
    {
        std::lock_guard<std::mutex> lock(socketMutex);
        activeSockets.erase(reinterpret_cast<uWS::WebSocket<false, true, PerSocketData>*>(ws));
    }
}



// ─────────────────────────────────────────────
// WebSocket Shutdown Control
// ─────────────────────────────────────────────

us_listen_socket_t* listenSocket = nullptr;

void stopWebSocketManagerWorker() {
    std::unique_lock<std::mutex> lock(socketMutex); 

        std::cout << "\n" << color("log") <<"Shutdown requested...\n";
        std::cout << "Closing " << activeSockets.size() << " connections..." << std::endl;
        us_listen_socket_close(0, listenSocket);
        auto socketsCopy = activeSockets;

    lock.unlock();

    for (auto* ws : socketsCopy) {
        if (ws) ws->end(1000, "Server shutdown");
    }
}