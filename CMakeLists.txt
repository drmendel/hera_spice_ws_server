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
    PATHS ${CSPICE_DIR}/include /usr/include/cspice /usr/local/include/cspice
    DOC "Path to CSPICE include directory"
)

# Try to find CSPICE libraries
find_library(CSPICE_LIB NAMES cspice.a libcspice
    PATHS ${CSPICE_DIR}/lib /usr/lib/cspice /usr/local/lib/cspice
    DOC "Path to CSPICE library"
)

find_library(CSPLIB_LIB NAMES csupport.a libcsupport
    PATHS ${CSPICE_DIR}/lib /usr/lib/cspice /usr/local/lib/cspice
    DOC "Path to CSPICE support library"
)

# Ensure paths were found
if(NOT CSPICE_INCLUDE_DIR OR NOT CSPICE_LIB OR NOT CSPLIB_LIB)
    message(FATAL_ERROR "CSPICE libraries or includes not found! Set CSPICE_DIR manually if needed.")
endif()

# ########################################### uWebSockets ############################################

# find_package(uWebSockets REQUIRED)
find_path(uWEBSOCKET_INCLUDE_DIR NAMES uWebSockets
    PATHS /include /usr/include /usr/local/include
    DOC "Path to CSPICE include directory"
)

find_library(uWEBSOCKET_LIB NAMES uSockets.a
    PATHS /usr/lib /usr/local/lib
    DOC "Path to CSPICE support library"
)

# ######################################### cURL + miniZip ###########################################

find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_library(MINIZIP_LIB minizip REQUIRED)

# ########################################## PROJECT SETUP ###########################################

# Add executable
file(GLOB_RECURSE SRC_FILES src/*.c src/*.cpp)
add_executable(hera_spice_websocket_server ${SRC_FILES})

# Add include directories (modern approach)
target_include_directories(hera_spice_websocket_server PRIVATE ${CSPICE_INCLUDE_DIR} ${uWEBSOCKET_INCLUDE_DIR} inc)

# Link libraries (CSPICE before system libs)
target_link_libraries(hera_spice_websocket_server PRIVATE ${CSPICE_LIB} ${CSPLIB_LIB} m ${uWEBSOCKET_LIB} CURL::libcurl ${MINIZIP_LIB} OpenSSL::Crypto ZLIB::ZLIB ssl crypto)

# Install target
install(TARGETS hera_spice_websocket_server DESTINATION bin)