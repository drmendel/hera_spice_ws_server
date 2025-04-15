#include <condition_variable>

#include <data_manager.hpp>
#include <spice_core.hpp>
#include <server_threads.hpp>
#include <websocket_manager.hpp>
#include <utils.hpp>

// ─────────────────────────────────────────────
// SPICE Kernel Update Thread - DataManager
// ─────────────────────────────────────────────

std::mutex versionMutex;
std::condition_variable versionCondition;
std::atomic<bool> shouldDataManagerRun;

void signalSpiceDataAvailable() {
    {
        std::unique_lock<std::mutex> lock(spiceMutex);
        spiceDataAvailable = true;
    }
    spiceCondition.notify_all();
}

void signalSpiceDataUnavailable() {
    {
        std::lock_guard<std::mutex> lock(spiceMutex);
        spiceDataAvailable = false;
    }
    spiceCondition.notify_all();
    deinitSpiceCore();
}

void dataManagerWorker(int hoursToWait) {
    DataManager dataManager;
    bool newVersionAvailable;

    if(dataManager.getLocalVersion() !=  "default") {
        initSpiceCore();
        signalSpiceDataAvailable();
    }

    while (true) {
        std::unique_lock<std::mutex> lock(versionMutex);
        
        // Wait until a new kernel version is available or thread shutdown requested
        newVersionAvailable = dataManager.isNewVersionAvailable();  // Only call once
        versionCondition.wait_for(lock, std::chrono::hours(hoursToWait), [&]() {
            return !shouldDataManagerRun.load() || newVersionAvailable;
        });

        if (!shouldDataManagerRun.load()) return; // Exit if thread shutdown requested
        if(!newVersionAvailable) continue;    // Continue if no new version available

        dataManager.downloadZipFile();
        dataManager.unzipZipFile();
        dataManager.deleteZipFile();
        dataManager.editTempMetaKernelFiles();
        dataManager.editTempVersionFile();

        signalSpiceDataUnavailable();
        deinitSpiceCore();      
        dataManager.moveFolder(); // WebSocket responses are disabled while SPICE data is being updated.
        initSpiceCore();
        signalSpiceDataAvailable();        
        
        dataManager.updateLocalVersion();
    }
}

// ─────────────────────────────────────────────
// uWebSocket Thread - WebSocketManager
// ─────────────────────────────────────────────

void webSocketManagerWorker(int port) {
    uWS::App threadApp;
    loop = uWS::Loop::get();
    threadApp.ws<PerSocketData>("/", {
        .open = onOpen,
        .message = onMessage,
        .close = onClose
    }).listen(port, [port](auto *token) {
        listenSocket = token;
        if (token) {
            std::cout << color("log") << "\nServer listening on port " << port << ".\n" << std::endl;
        } else {
            std::cerr << color("error") << "\nFailed to listen on port " << port << ".\n" << std::endl;
            exit(1);
        }
    });
    app = &threadApp;
    threadApp.run();
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
    shouldDataManagerRun.store(false);
    versionCondition.notify_all();
    shutdownServer();
    shuttingDown.store(true);
    shutdownCV.notify_all();

    if(dataManagerPointer->joinable()) dataManagerPointer->join();
    if(webSocketManagerPointer->joinable()) webSocketManagerPointer->join();
    deinitSpiceCore();

    std::cout << color("reset") << "\nServer stopped gracefully!\n";
    std::exit(0);
}

void handleSignal(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        gracefulShutdown(dataManagerPointer, webSocketManagerPointer);
    }
}