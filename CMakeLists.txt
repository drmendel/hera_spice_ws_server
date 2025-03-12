cmake_minimum_required(VERSION 3.16)
project(hera_spice_websocket_server)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ############################################## CSPICE ##############################################

# Specify CSPICE installation path if needed
set(CSPICE_DIR "cspice" CACHE PATH "Path to CSPICE installation")

# Try to find CSPICE include directory
find_path(CSPICE_INCLUDE_DIR NAMES SpiceUsr.h
    PATHS ${CSPICE_DIR}/include /usr/include /usr/local/include
    DOC "Path to CSPICE include directory")

# Try to find CSPICE libraries
find_library(CSPICE_LIB NAMES cspice.a
    PATHS ${CSPICE_DIR}/lib /usr/lib /usr/local/lib
    DOC "Path to CSPICE library")

find_library(CSPLIB_LIB NAMES csupport.a
    PATHS ${CSPICE_DIR}/lib /usr/lib /usr/local/lib
    DOC "Path to CSPICE support library")

# Ensure paths were found
if(NOT CSPICE_INCLUDE_DIR OR NOT CSPICE_LIB OR NOT CSPLIB_LIB)
    message(FATAL_ERROR "CSPICE libraries or includes not found! Set CSPICE_DIR manually if needed.")
endif()

# ########################################### PROJECT SETUP ###########################################

# Add include directories
include_directories(${CSPICE_INCLUDE_DIR} inc)

# Add source files
add_executable(hera_spice_websocket_server src/main.cpp src/spice_core.cpp)

# Link libraries
target_link_libraries(hera_spice_websocket_server ${CSPICE_LIB} ${CSPLIB_LIB} m)

# Install target
install(TARGETS hera_spice_websocket_server DESTINATION bin)