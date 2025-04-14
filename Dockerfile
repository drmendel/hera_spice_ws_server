FROM debian:bookworm-slim
LABEL Description="Dockerfile for building and running a C++ application"

# Installing dependencies
RUN apt-get update && apt-get install --no-install-recommends \
    git wget gzip ncompress \
    ca-certificates \
    build-essential cmake libssl-dev zlib1g-dev \
    libcurl4-openssl-dev \
    zlib1g-dev -y

# Installing cspice
RUN wget https://naif.jpl.nasa.gov/pub/naif/toolkit//C/PC_Linux_GCC_64bit/packages/cspice.tar.Z && \
    tar -xvf cspice.tar.Z && \
    rm cspice.tar.Z && \
    cd cspice && \
    mkdir -p /usr/local/include/cspice && \
    cp -r include/* /usr/local/include/cspice/ && \
    mkdir -p /usr/local/lib/cspice && \
    cp -r lib/* /usr/local/lib/cspice/ && \
    cd .. && \
    rm -vrf cspice

# Installing websockets
RUN git clone --recurse-submodules https://github.com/uNetworking/uWebSockets.git && \
    cd uWebSockets/uSockets && \
    WITH_OPENSSL=1 make -j$(nproc) && \
    cp src/libusockets.h /usr/local/include && \
    cp uSockets.a /usr/local/lib && \
    cd .. && \
    WITH_OPENSSL=1 make -j$(nproc) && \
    make install && \
    cd .. && \
    rm -vrf uWebSockets

# Installing minizip
RUN git clone https://github.com/nmoinvaz/minizip.git && \
    cd minizip && \
    mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc) && \  
    make install && \
    cd .. && \
    cd .. && \
    rm -vrf minizip && \
    ldconfig

# Copy application files
RUN mkdir -p /app/inc /app/src /app/data /app/certs
WORKDIR /app/inc
COPY inc/* .
WORKDIR /app/src
COPY src/* .
WORKDIR /app
COPY CMakeLists.txt .
WORKDIR /app/certs
RUN touch key.pem && cert.pem

# Copy data files
# COPY data ./data

# Build the application
RUN mkdir -p /app/build
WORKDIR /app/build
RUN cmake .. && make -j$(nproc)

# Environment variables
ENV PORT 8080
ENV HOURS 24
EXPOSE $PORT
ENV KEY /app/certs/key.pem
ENV CERT /app/certs/cert.pem

# Run the application
CMD sh -c "/app/build/hera_spice_websocket_server $PORT $HOURS $KEY $CERT $PASSPHRASE"
