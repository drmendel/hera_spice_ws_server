#ifndef HELPER_HPP
#define HELPER_HPP

// Standard C++ Libraries
#include <string>

void loadValues(int argc, char** argv, int& port, int& syncInterval);
void checkArgc(int argc, char** argv);
void printUsage(char** argv);
void printTitle();
void printExitOption();
std::string color(const std::string);
void userExit();

#endif // HELPER_HPP