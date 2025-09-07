#!/bin/sh
# $1 = arch
# $2 = sysroot

set -e

if [[ -d build-freetype ]]; then
    echo Assuming Freetype is up to date
    exit 0
fi 

# Download freetype
if [ ! -f freetype-2.4.9.tar.bz2 ]; then
    wget https://sourceforge.net/projects/freetype/files/freetype2/2.4.9/freetype-2.4.9.tar.bz2
fi

tar -xf freetype-2.4.9.tar.bz2
# rm freetype-2.4.9.tar.bz2

# Patch freetype
cd freetype-2.4.9
patch -p1 < ../freetype.patch
cd ..


# Build freetype
mkdir build-freetype || true
cd build-freetype
rm -rf * || true
chmod +x ../freetype-2.4.9/configure
../freetype-2.4.9/configure --host=$1-ethereal --prefix=/usr --build=x86_64-linux-gnu --without-zlib
make -j4
make DESTDIR=$2 install
cd ..