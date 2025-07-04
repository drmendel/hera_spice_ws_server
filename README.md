# HERA SPICE WEBSOCKET SERVER

`hera_spice_ws_server` is a high-performance WebSocket server that
implements a custom binary protocol for the distribution of telemetry
and ephemeris data derived from the European Space Agency’s HERA SPICE
kernels. The server delivers precise, time-stamped responses based on
locally maintained SPICE kernel files. It autonomously synchronizes
its kernel repository with ESA’s official database at user-configurable
intervals, thereby ensuring continuous availability of the most current
telemetry data to client applications.

## 🚀 Features

- Custom binary WebSocket protocol for SPICE-based telemetry queries  
- Periodic kernel synchronization from ESA HTTP servers
- Optimized for real-time simulation and visualization systems  

## ⚙️ Installation

A pre-built Docker image is available on Docker Hub:

```bash
docker pull drmendel/hera_spice_ws_server:latest
```

Alternatively, to build from source on Debian based linux distros:

```bash
git clone https://github.com/drmendel/hera_spice_ws_server.git
cd hera_spice_ws_server
./install_dependencies.sh
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## 🧭 Usage

### Start the Server

```bash
./hera_spice_ws_server <port> <syncInterval>
```

Example:

```bash
./hera_spice_ws_server 8080 3600
```

This starts the server on port `8080`, with kernel synchronization every 3600 seconds (1 hour).

### Stop the Server

Type `stop` in the terminal running the server to gracefully shut it down.

## 📡 Binary Protocol Specification

### Request Format (16 bytes total)

| Field        | Type     | Size (bytes) | Description                            |
|--------------|----------|--------------|----------------------------------------|
| Timestamp    | double   | 8            | Unix time in seconds (UTC)            |
| Mode         | char     | 1            | Query mode: 'i' or 'l'                |
| Observer ID  | int32_t  | 4            | Integer ID of the observer            |

**Request size:** 13 bytes

### Request Modes

- `'i'`: Instantaneous (uncorrected) state  
- `'l'`: Light-time and stellar aberration corrected (LT+S)  

### Response Format (variable size)

| Field           | Type     | Size (bytes)     | Description                            |
|-----------------|----------|------------------|--------------------------------------|
| Timestamp       | double   | 8                | Echoed Unix timestamp (seconds UTC) |
| Mode/Error Flag | char     | 1                | 'i', 'l', 'e', 'g', or 'h'  |
| Data Payload    | variable | 0 to 1620        | 0-15 ObjectData

**Minimum size:** 9 bytes (timestamp + error)  
**Maximum size:** 1629 bytes (timestamp + mode + 15 objects × 13 bytes each)

### Response Modes/Errors

- `'i'`: Instantaneous result
- `'l'`: LT+S corrected result
- `'e'`: General error (e.g., bad input)
- `'g'`: Invalid observer in 'i' mode  
- `'h'`: Invalid observer in 'l' mode  

#### ObjectData Structure (per object)

| Field           | Type      | Size (bytes)     | Description                   |
|-----------------|-----------|------------------|-------------------------------|
| objectId        | int32_t   | 4                | SPICE object identifier        |
| position        | double[3] | 24 (3 × 8 bytes) | Position vector (X, Y, Z)      |
| velocity        | double[3] | 24 (3 × 8 bytes) | Velocity vector (X, Y, Z)      |
| quaternion      | double[4] | 32 (4 × 8 bytes) | Orientation quaternion (w, x, y, z) |
| angularVelocity | double[3] | 24 (3 × 8 bytes) | Angular velocity vector         |

**Total size per object:** 108 bytes

## 🛰️ Notes

- Internet access is required for ESA kernel synchronization
- The binary protocol assumes little-endian byte order
- Tested on Ubuntu Linux
- Tested on Alpine Linux Docker image
- Minor modifications may be required for other systems

## Dependencies and Licenses

This project depends on the following third-party libraries and tools.

| Dependency   | License                                | Copyright / Notes                                               |
|--------------|--------------------------------------|----------------------------------------------------------------|
| **CSPICE**   | [NASA Open Source License](https://naif.jpl.nasa.gov/naif/rules.html) | © 2006-2025 California Institute of Technology. Use permitted under NASA’s open source license. |
| **uWebSockets** | [Apache License 2.0](https://github.com/uNetworking/uWebSockets/blob/master/LICENSE) | © 2019 uNetworking. Licensed under Apache 2.0.                 |
| **libcurl**  | [MIT/X-derivative](https://curl.se/docs/copyright.html) | © 1996-2025 Daniel Stenberg et al. Licensed under MIT/X-derivative. |
| **minizip-ng** | [BSD 3-Clause](https://github.com/nmoinvaz/minizip/blob/master/LICENSE) | © 2018 Nathan Moinvaziri. Licensed under BSD 3-Clause.          |
| **zlib**     | [zlib License](https://www.zlib.net/zlib_license.html) | © 1995-2025 Jean-loup Gailly and Mark Adler. Permissive license requiring attribution. |
| **OpenSSL**  | [Apache License 2.0](https://www.openssl.org/source/license.html) | © OpenSSL Project Authors. Licensed under Apache 2.0.           |

License files for all dependencies are included in the `licenses/` folder of this project.
