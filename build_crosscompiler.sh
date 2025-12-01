#!/bin/bash
set -e

# Configuration
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

BINUTILS_VERSION=2.41
GCC_VERSION=13.2.0

# Create directories
mkdir -p $PREFIX
mkdir -p ~/src

cd ~/src

# Download binutils
if [ ! -f "binutils-${BINUTILS_VERSION}.tar.xz" ]; then
    echo "Downloading binutils..."
    wget https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz
fi

# Download GCC
if [ ! -f "gcc-${GCC_VERSION}.tar.xz" ]; then
    echo "Downloading GCC..."
    wget https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz
fi

# Build binutils
echo "Building binutils..."
if [ ! -d "build-binutils" ]; then
    tar -xf binutils-${BINUTILS_VERSION}.tar.xz
    mkdir -p build-binutils
    cd build-binutils
    ../binutils-${BINUTILS_VERSION}/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
    make -j$(nproc)
    make install
    cd ..
fi

# Build GCC
echo "Building GCC..."
if [ ! -d "build-gcc" ]; then
    tar -xf gcc-${GCC_VERSION}.tar.xz
    mkdir -p build-gcc
    cd build-gcc
    ../gcc-${GCC_VERSION}/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
    make all-gcc -j$(nproc)
    make all-target-libgcc -j$(nproc)
    make install-gcc
    make install-target-libgcc
    cd ..
fi

echo ""
echo "Cross-compiler built successfully!"
echo "Add the following to your ~/.bashrc or ~/.profile:"
echo "export PATH=\"\$HOME/opt/cross/bin:\$PATH\""
