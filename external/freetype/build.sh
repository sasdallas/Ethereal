#!/bin/sh
# $1 = arch
# $2 = sysroot

set -e

export FT_VERSION=2.14.3

if [[ -d build-freetype ]]; then

    cd build-freetype
    make DESTDIR=$2 -j4 install
    cd ..

    exit 0
fi 

# Download freetype
if [ ! -f freetype-$FT_VERSION.tar.xz ]; then
    wget "https://downloads.sourceforge.net/freetype/freetype-$FT_VERSION.tar.xz"
fi

tar -xf freetype-$FT_VERSION.tar.xz

# Patch freetype
cd freetype-$FT_VERSION
patch -p1 < ../freetype-$FT_VERSION.patch
cd ..


# Build freetype
mkdir build-freetype || true
cd build-freetype
rm -rf * || true
chmod +x ../freetype-$FT_VERSION/configure
../freetype-$FT_VERSION/configure --host=$1-ethereal --prefix=/usr --build=x86_64-linux-gnu --without-zlib --enable-shared
make -j4
make DESTDIR=$2 install
cd ..
