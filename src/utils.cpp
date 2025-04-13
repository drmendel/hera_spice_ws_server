#include <chrono>

#include <utils.hpp>

void printUsage(char** argv) {
    std::cerr << color("reset") << "Usage: " << argv[0] << " <port> <interval>\n";
    std::cerr << "<port>    - The port number to run the server on.\n";
    std::cerr << "<interval> - The interval (in hours) between kernel version checks.\n";
}

void printTitle() {
    std::cout << color("reset");
    std::cout << " __________________________________________________________________________________ \n";
    std::cout << "|    _     _ _______  ______ _______      _______  _____  _____ _______ _______    |\n";
    std::cout << "|    |_____| |______ |_____/ |_____|      |______ |_____]   |   |       |______    |\n";
    std::cout << "|    |     | |______ |    \\_ |     |      ______| |       __|__ |_____  |______    |\n";
    std::cout << "|                                                                                  |\n";
    std::cout << "|    _  _  _ _______      _______ _______ _    _ _______  ______                   |\n";
    std::cout << "|    |  |  | |______      |______ |______  \\  /  |______ |_____/                   |\n";
    std::cout << "|    |__|__| ______|      ______| |______   \\/   |______ |    \\_                   |\n";
    std::cout << "|__________________________________________________________________________________|\n";
}

std::string color(const std::string type) {
    if (type == "info")        return "\033[36m";  // cyan
    if (type == "connect")     return "\033[32m";  // green
    if (type == "disconnect")  return "\033[31m";  // red
    if (type == "warn")        return "\033[33m";  // yellow
    if (type == "error")       return "\033[91m";  // light red
    return "\033[0m";
}
