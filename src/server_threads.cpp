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
// DataManagerWorker - Kernel Update Thread
// ─────────────────────────────────────────────

void dataManagerWorker(int syncInterval) {
    DataManager dataManager;

    if(dataManager.getLocalVersion() !=  NO_VERSION) {
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
// WebSocketManagerWorker - uWebSockets Thread
// ─────────────────────────────────────────────

void webSocketManagerWorker(int port) {
    uWS::App threadApp;
    threadApp.ws<UserData>(ENTRY_POINT, {
        .open = onOpen,
        .message = onMessage,
        .close = onClose
    }).listen(port, [port](us_listen_socket_t* socket) {
        listenSocket = socket;
        if (socket) {
            std::cout << color("log") << "\nServer listening on port " << port << ".\n\n"
                      << std::flush;
        } else {
            std::cerr << color("error") << "\nFailed to listen on port " << port << ".\n\n"
                      << std::flush;
            exit(ERR_SOCKET_NULL);
        }
    }).run();
}



// ─────────────────────────────────────────────
// Graceful Shutdown - stop worker threads
// ─────────────────────────────────────────────

std::atomic<bool> shuttingDown = false;
std::thread* dataManagerPointer = nullptr;
std::thread* webSocketManagerPointer = nullptr;

void gracefulShutdown(std::thread* dataManagerPointer, std::thread* webSocketManagerPointer) {
    if (shuttingDown.exchange(true)) return;

    stopDataManagerWorker();
    stopWebSocketManagerWorker();
    if(dataManagerPointer->joinable()) dataManagerPointer->join();
    if(webSocketManagerPointer->joinable()) webSocketManagerPointer->join();

    std::cout << color("log") << "\nServer stopped gracefully!\n" << std::flush;
}

void handleSignal(int signal) {
    if (signal == SIGTERM) {
        gracefulShutdown(dataManagerPointer, webSocketManagerPointer);
        std::exit(SUCCESSFUL_EXIT);
    }
}
