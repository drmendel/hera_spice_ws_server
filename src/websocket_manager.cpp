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
std::unordered_set<uWS::WebSocket<false, true, UserData>*> activeSockets;



// ─────────────────────────────────────────────
// WebSocket Event Handlers
// ─────────────────────────────────────────────

void onOpen(WS* ws) {
    UserData* data = ws->getUserData();
    if (!data) return; 

    data->id = idAllocator.allocate();
    activeConnections.fetch_add(1, std::memory_order_relaxed);

    std::cout << color("connect")
              << "Client connected with ID:    [" << data->id << "]\n"
              << color("log")
              << "Active connections:          [" << activeConnections.load()
              << "]\n\n" << std::flush;

    std::lock_guard<std::mutex> lock(socketMutex);
    activeSockets.insert(ws);
}

void onMessage(WS* ws, std::string_view message, uWS::OpCode opCode) {
    if (message.length() != EXPECTED_MESSAGE_LENGTH) {
        ws->send(message, opCode);
        return;
    }

    std::unique_lock<std::mutex> lock(spiceMutex);
    spiceCondition.wait(lock, [] { return spiceDataAvailable.load(); });

    RequestHandler requestHandler(message);
    lock.unlock();

    ws->send(requestHandler.getMessage(), opCode);

    #ifdef DEBUG
        printResponse(requestHandler.getMessage());
    #endif
}

void onClose(WS* ws, int code, std::string_view message) {
    UserData* data = ws->getUserData();
    if (!data) return;

    idAllocator.release(data->id);
    activeConnections.fetch_sub(1, std::memory_order_relaxed);

    std::cout << color("disconnect")
              << "Client disconnected with ID: [" << data->id << "]\n"
              << color("log")
              << "Active connections:          [" << activeConnections.load() << "]\n\n"
              << std::flush;

    std::lock_guard<std::mutex> lock(socketMutex);
    activeSockets.erase(ws);
}



// ─────────────────────────────────────────────
// WebSocket Shutdown Control
// ─────────────────────────────────────────────

us_listen_socket_t* listenSocket = nullptr;

void stopWebSocketManagerWorker() {
    std::cout << color("log") <<"WebSocketManager shutdown requested.\n"
              << "Closing " << activeSockets.size() << " connections...\n"
              << std::flush;

    std::unique_lock<std::mutex> lock(socketMutex);
    if(listenSocket) us_listen_socket_close(0, listenSocket);
    auto socketsCopy = activeSockets;
    lock.unlock();

    for (auto* ws : socketsCopy) {
        if (ws) ws->end(1000, "Server shutdown");
    }
}
