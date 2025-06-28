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

#ifndef SERVER_THREADS_HPP
#define SERVER_THREADS_HPP

// Standard C++ Libraries
#include <condition_variable>
#include <csignal>

// Project Headers
#include <websocket_manager.hpp>

#define ENTRY_POINT "/ws/"

// ─────────────────────────────────────────────
// SPICE Kernel Update Thread - DataManager
// ─────────────────────────────────────────────

/*  
 * Periodically checks for a new Hera kernel version every 'syncInterval' seconds.  
 * If an update is available, downloads, extracts, modifies and replaces the kernel files.  
 * Ensures thread-safe updates by signaling data availability changes.
 */
void dataManagerWorker(int syncInterval);

// ─────────────────────────────────────────────
// uWebSocket Thread - WebSocketManager
// ─────────────────────────────────────────────

/*
 * WebSocket manager worker function that initializes and runs the WebSocket server.  
 * Listens on the specified port and handles client connections, messages, and disconnections.
 * Ensures thread-safe data access to spice kernel files.
 */
void webSocketManagerWorker(int port);

// ─────────────────────────────────────────────
// Graceful Shutdown - stop worker threads
// ─────────────────────────────────────────────

extern std::atomic<bool> shuttingDown;
extern std::mutex shutdownMutex;
extern std::condition_variable shutdownCV;

extern std::thread* dataManagerPointer;
extern std::thread* webSocketManagerPointer;

void gracefulShutdown(std::thread* dataManagerPointer, std::thread* webSocketManagerPointer);
void handleSignal(int signal);

#endif // SERVER_THREADS_HPP
