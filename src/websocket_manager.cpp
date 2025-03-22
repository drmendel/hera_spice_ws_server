#include <condition_variable>
#include <unordered_set>
#include <atomic>
#include <queue>

#include <uWebSockets/App.h>

#include <websocket_manager.hpp>
#include <spice_core.hpp>

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

// WebSocket event handlers
void onOpen(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws) {
    PerSocketData* data = ws->getUserData();
    if (data) {
        data->id = idAllocator.allocate();
        activeConnections.fetch_add(1, std::memory_order_relaxed);
        std::cout << "Client connected with [ID: " << data->id << "]" << std::endl;
    }
    std::cout << "Active connections: " << activeConnections.load() << std::endl;
}

void onMessage(uWS::WebSocket<false, true, PerSocketData> *ws, std::string_view message, uWS::OpCode opCode) {

    // Invalid message: echo back & return
    if (message.length() != (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t))) { 
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
        std::cout << "Client disconnected [ID: " << data->id << "]" << std::endl;
    }
}