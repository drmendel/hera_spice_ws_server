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

std::mutex socketMutex;
std::condition_variable socketCondition;
std::atomic<bool> shouldWebSocketRun = true;

#include <fstream>

void webSocketManagerWorker(int port, const std::string& key, const std::string& cert, const std::string& pwd) {

    std::cout << "KEY PATH: " << key << "\n";
    std::cout << "CERT PATH: " << cert << "\n";
    std::cout << "PWD: " << pwd << "\n";

    std::ifstream k(key.c_str());
    std::ifstream c(cert.c_str());

    std::cout << "KEY readable: " << k.good() << ", CERT readable: " << c.good() << "\n";


    uWS::SSLApp threadApp({
        .key_file_name = key.c_str(),
        .cert_file_name = cert.c_str(),
        .passphrase = pwd.c_str()
    });
    loop = uWS::Loop::get();
    threadApp.ws<PerSocketData>("/*", {
        .maxBackpressure = 100 * 1024 * 1024,
        .closeOnBackpressureLimit = false,
        .resetIdleTimeoutOnSend = false,
        .sendPingsAutomatically = true,
        
        .open = onOpen,
        .message = onMessage,
        .close = onClose
    }).listen(port, [port](auto *socket) {
        listenSocket = socket;
        if (socket) {
            std::cout << color("log") << "Server listening on port " << port << ".\n" << color("reset") << std::endl;
        } else {
            std::cerr << color("error") << "Failed to listen on port " << port << ".\n" << color("reset") << std::endl;
            std::lock_guard<std::mutex> lock(shutdownMutex);
            shouldWebSocketRun.store(false);
            socketCondition.notify_all();
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

void gracefulShutdown() {
    if (shuttingDown.exchange(true)) return;
    shouldDataManagerRun.store(false);
    versionCondition.notify_all();
    shutdownServer();
    shuttingDown.store(true);
    shutdownCV.notify_all();
}

void handleSignal(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        gracefulShutdown();
    }
}