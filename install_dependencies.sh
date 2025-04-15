#!/bin/bash
if [[ $EUID -ne 0 ]]; then
    exec sudo "$0" "$@"
fi

# Install CSPICE
echo
echo "############################## INSTALLING CSPICE... 🛰️  ##############################"
sleep 0.5
sudo apt install wget gzip ncompress -y

wget https://naif.jpl.nasa.gov/pub/naif/toolkit//C/PC_Linux_GCC_64bit/packages/cspice.tar.Z
tar -xvf cspice.tar.Z
rm cspice.tar.Z
cd cspice

sudo mkdir /usr/local/include/cspice
sudo cp -r include/* /usr/local/include/cspice
sudo mkdir /usr/local/lib/cspice
sudo cp -r lib/* /usr/local/lib/cspice

cd ..
rm -vr cspice

# Install uWebsockets
echo
echo "############################## INSTALLING uWEBSOCKETS... 🌐 ##############################"
sleep 0.5

sudo apt install build-essential cmake libssl-dev zlib1g-dev libzstd-dev -y

git clone --recurse-submodules https://github.com/uNetworking/uWebSockets.git

cd uWebSockets/uSockets
make
# ar rvs uSockets.a *.o
sudo cp uSockets.a /usr/local/lib/
sudo cp src/libusockets.h /usr/local/include/

cd ..
make -j$(nproc)
sudo make install

cd ..
rm -vr uWebSockets

# Install curl
echo
echo "############################## INSTALLING CURL... 📥 ##############################"
sleep 0.5

sudo apt install libcurl4-openssl-dev -y

# Install minizip
echo
echo "############################## INSTALLING MINIZIP... 📦 ##############################"
sleep 0.5

sudo apt install zlib1g-dev

git clone https://github.com/nmoinvaz/minizip.git  
cd minizip  
mkdir build && cd build  
cmake ..  
make -j$(nproc)  
sudo make install
cd ..
cd ..
rm -vr minizip

# Refresh lib config
sudo ldconfig
echo
echo "############################## FINISHED 🏁 ##############################"