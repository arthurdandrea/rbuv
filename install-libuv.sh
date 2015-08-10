#!/bin/sh
set -ex
LIBUV=1.7.0
wget https://github.com/libuv/libuv/archive/v${LIBUV}.tar.gz --output-document libuv-${LIBUV}.tar.gz
tar -xf libuv-${LIBUV}.tar.gz
mv libuv-${LIBUV} libuv
cd libuv
./autogen.sh
./configure --prefix=$PWD/pkg --disable-shared --enable-static
make
make install
