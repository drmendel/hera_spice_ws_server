// Standard C++ Libraries
#include <condition_variable>
#include <csignal>
#include <chrono>
#include <mutex>

// External Libraries
#include <uWebSockets/App.h>

// Project Headers
#include <websocket_manager.hpp>
#include <server_threads.hpp>
#include <data_manager.hpp>
#include <spice_core.hpp>
#include <utils.hpp>



// ─────────────────────────────────────────────
// SPICE Kernel Update Thread - DataManager
// ─────────────────────────────────────────────

void dataManagerWorker(int syncInterval) {
    DataManager dataManager;

    if(dataManager.getLocalVersion() !=  "default") {
        dataManager.makeSpiceDataAvailable();
    }

    while (true) {    
        if(dataManager.isNewVersionAvailable()) {
            if(!dataManager.downloadZipFile()) continue;            // Continue if download failed
            if(!dataManager.unzipZipFile()) continue;               // Continue if unzip failed
            if(!dataManager.editTempMetaKernelFiles()) continue;    // Continue if editing meta-kernel files failed;
            if(!dataManager.editTempVersionFile()) continue;        // Continue if editing version file failed
            if(!dataManager.deleteUnUsedFiles()) continue;          // Continue if deleting unneeded files failed

            dataManager.makeSpiceDataUnavailable();                 // Notify webSocket thread then unload SPICE data
            dataManager.moveFolder();                               // WebSocket responses are disabled while SPICE data is being updated.
            dataManager.makeSpiceDataAvailable();                   // Load new SPICE data then notify webSocket thread
            
            dataManager.deleteTmpFolder();                          // Delete the temporary folder
        }
        
        std::unique_lock<std::mutex> lock(versionMutex);
        versionCondition.wait_for(lock, std::chrono::seconds(syncInterval), [&]() {
            return !shouldDataManagerRun.load();
        });

        if(!shouldDataManagerRun.load()) {
            dataManager.makeSpiceDataUnavailable();
            break;                                                  // Exit if thread shutdown requested
        }
    }

    return;
}



// ─────────────────────────────────────────────
// uWebSocket Thread - WebSocketManager
// ─────────────────────────────────────────────

void webSocketManagerWorker(int port) {
    uWS::App threadApp;
    threadApp.ws<PerSocketData>("/ws/", {
        .open = onOpen,
        .message = onMessage,
        .close = onClose
    }).listen(port, [port](auto *token) {
        listenSocket = token;
        if (token) std::cout << color("log") << "\nServer listening on port " << port << ".\n" << std::endl;
        else {
            std::cerr << color("error") << "\nFailed to listen on port " << port << ".\n" << std::endl;
            exit(1);
        }
    }).run();
}



// ─────────────────────────────────────────────
// Graceful Shutdown - stop worker threads
// ─────────────────────────────────────────────

std::atomic<bool> shuttingDown = false;
std::mutex shutdownMutex;
std::condition_variable shutdownCV;

std::thread* dataManagerPointer = nullptr;
std::thread* webSocketManagerPointer = nullptr;

void gracefulShutdown(std::thread* dataManagerPointer, std::thread* webSocketManagerPointer) {
    if (shuttingDown.exchange(true)) return;
    shuttingDown.store(true);
    shutdownCV.notify_all();

    stopDataManagerWorker();
    stopWebSocketManagerWorker();
    if(dataManagerPointer->joinable()) dataManagerPointer->join();
    if(webSocketManagerPointer->joinable()) webSocketManagerPointer->join();

    std::cout << color("reset") << "\nServer stopped gracefully!\n";
}

void handleSignal(int signal) {
    if (signal == SIGTERM) {
        gracefulShutdown(dataManagerPointer, webSocketManagerPointer);
        std::exit(SUCCESSFUL_EXIT);
    }
}
