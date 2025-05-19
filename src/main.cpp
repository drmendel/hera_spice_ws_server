// Project headers
#include <server_threads.hpp>
#include <utils.hpp>



int main(int argc, char* argv[]) {
    checkArgc(argc, argv);
    
    int port;
    int syncInterval;
    loadValues(argc, argv, port, syncInterval);

    printTitle();
    printExitOption();
    
    shouldDataManagerRun.store(true);
    std::thread dataManagerThread(dataManagerWorker, syncInterval);
    std::thread webSocketManagerThread(webSocketManagerWorker, port);
    dataManagerPointer = &dataManagerThread;
    webSocketManagerPointer = &webSocketManagerThread;

    std::signal(SIGTERM, handleSignal);
    userExit();
    gracefulShutdown(dataManagerPointer, webSocketManagerPointer);
}
