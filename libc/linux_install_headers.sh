#!/usr/bin/bash

set -e


if ! [ -d linux-6.16 ]; then
    wget https://www.kernel.org/pub/linux/kernel/v6.x/linux-6.16.tar.xz
    tar -xf linux-6.16.tar.xz
fi


cd linux-6.16

make headers_install ARCH=$BUILD_ARCH INSTALL_HDR_PATH=$DESTDIR$PREFIX/