// Standard C++ Libraries
#include <iostream>
#include <chrono>

// Project headers
#include <utils.hpp>
#include <server_threads.hpp>



// ─────────────────────────────────────────────
// Utility functions
// ─────────────────────────────────────────────

void checkArgc(int argc, char** argv) {
    if (argc < 3) {
        printUsage(argv);
        std::exit(ERR_INVALID_ARGUMENTS);
    }
}

void loadValues(int argc, char** argv, int& port, int& syncInterval) {
    try {
        port = std::stoi(argv[1]);
        int tmp = std::stoi(argv[2]);
        syncInterval = tmp > 0 ? tmp : 1;
        return;
    } catch (const std::invalid_argument& e) {
        std::cerr << color("error") << "\nError: Port and interval must be valid integers.\n";

    } catch (const std::out_of_range& e) {
        std::cerr << color("error") << "\nError: Port or interval value out of range.\n";
    }
    printUsage(argv);
    std::exit(ERR_INVALID_ARGUMENTS);
}

void printUsage(char** argv) {
    std::cerr << color("reset") << "Usage: " << argv[0] << " <port> <syncInterval>\n";
    std::cerr << "<port>         - The port number to run the server on.\n";
    std::cerr << "<syncInterval> - The interval (in seconds) between kernel version checks.\n";
}

void printTitle() {
    std::cout << color("log");
    std::cout << " ____________________________________________________________________________ \n";
    std::cout << "| _     _ _______  ______ _______      _______  _____  _____ _______ _______ |\n";
    std::cout << "| |_____| |______ |_____/ |_____|      |______ |_____]   |   |       |______ |\n";
    std::cout << "| |     | |______ |    \\_ |     |      ______| |       __|__ |_____  |______ |\n";
    std::cout << "|                                                                            |\n";
    std::cout << "| _  _  _ _______      _______ _______ _    _ _______  ______                |\n";
    std::cout << "| |  |  | |______      |______ |______  \\  /  |______ |_____/                |\n";
    std::cout << "| |__|__| ______|      ______| |______   \\/   |______ |    \\_                |\n";
    std::cout << "|____________________________________________________________________________|\n";
}

void printExitOption() {
    std::cout << color("info") << "\nType `exit` to stop the server!" << color("log") << std::endl;
}

std::string color(const std::string type) {
    if (type == "info")        return "\033[36m";  // cyan
    if (type == "connect")     return "\033[32m";  // green
    if (type == "disconnect")  return "\033[31m";  // red
    if (type == "warn")        return "\033[33m";  // yellow
    if (type == "error")       return "\033[91m";  // light red
    return "\033[0m";                              // white
}

void waitForExitCommand() {
    std::string command = "notExit";
    while (!shuttingDown.load()) {
        std::cin >> command;
        if (command == "exit") break;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}
