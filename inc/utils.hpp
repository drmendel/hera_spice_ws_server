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

#ifndef HELPER_HPP
#define HELPER_HPP

// Standard C++ Libraries
#include <string>

// ─────────────────────────────────────────────
// Compile options
// ─────────────────────────────────────────────
#define DOCKER
// #define DEBUG

// ─────────────────────────────────────────────
// Error options
// ─────────────────────────────────────────────
#define SUCCESSFUL_EXIT 0
#define ERR_INVALID_ARGUMENTS 1
#define ERR_SOCKET_NULL 2
#define ERR_FORCED_SHUTDOWN 3

// ─────────────────────────────────────────────
// Defined values
// ─────────────────────────────────────────────
#define NO_VERSION "no_version"
#define EXPECTED_MESSAGE_LENGTH 13

// ─────────────────────────────────────────────
// Utility functions
// ─────────────────────────────────────────────
void loadValues(int argc, char** argv, int& port, int& syncInterval);
void checkArgc(int argc, char** argv);
void printUsage(char** argv);
void printTitle();
void printExitOption();
std::string color(const std::string);
void waitForExitCommand();

#endif // HELPER_HPP
