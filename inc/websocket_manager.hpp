#ifndef WEBSOCKET_MANAGER_HPP
#define WEBSOCKET_MANAGER_HPP

// C++ Standard Libraries
#include <condition_variable>
#include <queue>
#include <unordered_set>
#include <mutex>
#include <atomic>

// External Libraries
#include <uWebSockets/App.h>

// ─────────────────────────────────────────────
// Synchronization for Message Waiting
// ─────────────────────────────────────────────
extern std::mutex spiceMutex;
extern std::condition_variable spiceCondition;
extern std::atomic<bool> spiceDataAvailable;

// ─────────────────────────────────────────────
// Connection ID Allocator
// ─────────────────────────────────────────────
class ConnectionIDAllocator {
public:
    uint64_t allocate();
    void release(uint64_t id);
private:
    std::mutex mutex;
    std::queue<uint64_t> availableIDs;
    std::unordered_set<uint64_t> activeIDs;
    uint64_t nextID = 1;
};
extern std::atomic<int> activeConnections;
extern ConnectionIDAllocator idAllocator;

// ─────────────────────────────────────────────
// WebSocket Connection Data
// ─────────────────────────────────────────────
struct PerSocketData {
    uint64_t id;
};
extern std::mutex socketMutex;
extern std::unordered_set<uWS::WebSocket<false, true, PerSocketData>*> activeSockets;

// ─────────────────────────────────────────────
// WebSocket Event Handlers
// ─────────────────────────────────────────────
void onOpen(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws);
void onMessage(uWS::WebSocket<false, true, PerSocketData> *ws, std::string_view message, uWS::OpCode opCode);
void onClose(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws, int code, std::string_view message);

// ─────────────────────────────────────────────
// WebSocket Shutdown Control
// ─────────────────────────────────────────────
extern uWS::App* app;
extern uWS::Loop* loop;
extern us_listen_socket_t* listenSocket;
void shutdownServer();

#endif // WEBSOCKET_MANAGER_HPP
