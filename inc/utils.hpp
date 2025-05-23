#ifndef HELPER_HPP
#define HELPER_HPP

// Standard C++ Libraries
#include <string>

// #define DEBUG
// #define DOCKER

#define SUCCESSFUL_EXIT 0
#define ERR_INVALID_ARGUMENTS 1

#define NO_VERSION "no_version"

void loadValues(int argc, char** argv, int& port, int& syncInterval);
void checkArgc(int argc, char** argv);
void printUsage(char** argv);
void printTitle();
void printExitOption();
std::string color(const std::string);
void waitForExitCommand();

#endif // HELPER_HPP