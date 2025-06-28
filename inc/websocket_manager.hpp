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

#ifndef WEBSOCKET_MANAGER_HPP
#define WEBSOCKET_MANAGER_HPP

// Standard C++ Libraries
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
struct UserData {
    uint64_t id;
};
using WS = uWS::WebSocket<false, uWS::SERVER, UserData>;
extern std::mutex socketMutex;
extern std::unordered_set<WS*> activeSockets;

// ─────────────────────────────────────────────
// WebSocket Event Handlers
// ─────────────────────────────────────────────
void onOpen(WS *ws);
void onMessage(WS *ws, std::string_view message, uWS::OpCode opCode);
void onClose(WS *ws, int code, std::string_view message);

// ─────────────────────────────────────────────
// WebSocket Shutdown Control
// ─────────────────────────────────────────────
extern us_listen_socket_t* listenSocket;
void stopWebSocketManagerWorker();

#endif // WEBSOCKET_MANAGER_HPP
