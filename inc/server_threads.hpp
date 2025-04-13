#ifndef SERVER_THREADS_HPP
#define SERVER_THREADS_HPP

// C++ Standard Libraries
#include <condition_variable>
#include <csignal>

// Project headers
#include <websocket_manager.hpp>

// ─────────────────────────────────────────────
// SPICE Kernel Update Thread - DataManager
// ─────────────────────────────────────────────

extern std::mutex versionMutex;
extern std::condition_variable versionCondition;
extern std::atomic<bool> shouldDataManagerRun;

void signalSpiceDataAvailable();
void signalSpiceDataUnavailable();

/*  
 * Periodically checks for a new Hera kernel version every 24 hours.  
 * If an update is available, downloads, extracts, and replaces the kernel files.  
 * Ensures thread-safe updates by signaling data availability changes.  
 */
void dataManagerWorker(int hoursToWait);

// ─────────────────────────────────────────────
// uWebSocket Thread - WebSocketManager
// ─────────────────────────────────────────────

/*
 * WebSocket manager worker function that initializes and runs the WebSocket server.  
 * Listens on the specified port and handles client connections, messages, and disconnections.
 * Ensures thread-safe data access (spice kernels) and signaling.
 */
void webSocketManagerWorker(int port);

// ─────────────────────────────────────────────
// Graceful Shutdown - stop worker threads
// ─────────────────────────────────────────────

extern std::atomic<bool> shuttingDown;
extern std::mutex shutdownMutex;
extern std::condition_variable shutdownCV;

void gracefulShutdown();
void handleSignal(int signal);

#endif // SERVER_THREADS_HPP