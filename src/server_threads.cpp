#include <condition_variable>

#include <data_manager.hpp>
#include <spice_core.hpp>
#include <server_threads.hpp>
#include <websocket_manager.hpp>

// ─────────────────────────────────────────────
// SPICE Kernel Update Thread - DataManager
// ─────────────────────────────────────────────

std::mutex versionMutex;
std::condition_variable versionCondition;
std::atomic<bool> shouldDataManagerRun;

void signalSpiceDataAvailable() {
    initSpiceCore();
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
    std::cout << "DM Worker Stoped!" << std::endl;
}

// ─────────────────────────────────────────────
// uWebSocket Thread - WebSocketManager
// ─────────────────────────────────────────────

void webSocketManagerWorker(int port) {
    uWS::App app;
    
    app.ws<PerSocketData>("/", {
        .open = onOpen,
        .message = onMessage,
        .close = onClose
    }).listen(port, [port](auto *token) {
        if (token) {
            std::cout << "Server listening on port " << port << std::endl;
        } else {
            std::cerr << "Failed to listen on port " << port << std::endl;
            exit(1);
        }
    });
    
    app.run();
}