# 
#   Copyright 2025 Mendel Dobondi-Reisz
# 
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
# 
#       http://www.apache.org/licenses/LICENSE-2.0
# 
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

# Stage 1: Build
FROM alpine:latest AS builder

RUN apk add --no-cache g++ cmake make git openssl-dev zlib-dev curl-dev wget tar gzip zstd-dev

# CSPICE
RUN wget https://naif.jpl.nasa.gov/pub/naif/toolkit//C/PC_Linux_GCC_64bit/packages/cspice.tar.Z && \
    tar -xvf cspice.tar.Z && \
    mkdir -p /usr/local/include/cspice && \
    cp -r cspice/include/* /usr/local/include/cspice/ && \
    mkdir -p /usr/local/lib/cspice && \
    cp -r cspice/lib/* /usr/local/lib/cspice/ && \
    rm -rf cspice*

# uWebSockets
RUN git clone --recurse-submodules https://github.com/uNetworking/uWebSockets.git && \
    cd uWebSockets/uSockets && make && \
    cp src/libusockets.h /usr/local/include && \
    cp uSockets.a /usr/local/lib && \
    cd .. && make -j$(nproc) && make install && \
    cd ../ && rm -rf uWebSockets

# minizip
RUN git clone https://github.com/nmoinvaz/minizip.git && \
    mkdir -p minizip/build && cd minizip/build && \
    cmake .. && make -j$(nproc) && make install && \
    cd ../.. && rm -rf minizip

# App build
WORKDIR /app
COPY inc ./inc
COPY src ./src
COPY CMakeLists.txt .
RUN cmake -B build -S . -DDOCKER=1 && cmake --build build -j$(nproc)

# Stage 2: Final image
FROM alpine:latest

RUN apk add --no-cache libstdc++ openssl zlib curl zstd-dev

COPY --from=builder /usr/local/lib/cspice /usr/local/lib/cspice
COPY --from=builder /usr/local/include/cspice /usr/local/include/cspice
COPY --from=builder /usr/local/lib/libminizip.* /usr/local/lib/
COPY --from=builder /usr/local/lib/libcrypto* /usr/local/lib/
COPY --from=builder /usr/local/lib/libssl* /usr/local/lib/
COPY --from=builder /usr/local/lib/uSockets.a /usr/local/lib/
COPY --from=builder /app/build/hera_spice_websocket_server /app/hera_spice_websocket_server

ENV PORT 80
ENV SYNC_INTERVAL 3600
EXPOSE $PORT

CMD /app/hera_spice_websocket_server $PORT $SYNC_INTERVAL
