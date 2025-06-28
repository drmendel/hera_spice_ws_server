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
#include <thread>
#include <csignal>

// Project headers
#include <server_threads.hpp>
#include <utils.hpp>



// ─────────────────────────────────────────────
// Main - Entry Point
// ─────────────────────────────────────────────

int main(int argc, char* argv[]) {
    checkArgc(argc, argv);
    
    int port;
    int syncInterval;
    loadValues(argc, argv, port, syncInterval);

    printTitle();
    printExitOption();
    
    std::thread dataManagerThread(dataManagerWorker, syncInterval);
    std::thread webSocketManagerThread(webSocketManagerWorker, port);
    
    dataManagerPointer = &dataManagerThread;
    webSocketManagerPointer = &webSocketManagerThread;
    std::signal(SIGTERM, handleSignal);

    waitForExitCommand();
    gracefulShutdown(dataManagerPointer, webSocketManagerPointer);
    
    return SUCCESSFUL_EXIT;
}
