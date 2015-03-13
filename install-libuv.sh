#!/bin/sh
set -ex
LIBUV=1.4.2
wget https://github.com/libuv/libuv/archive/v${LIBUV}.tar.gz --output-document libuv-${LIBUV}.tar.gz
tar -xf libuv-${LIBUV}.tar.gz
mv libuv-${LIBUV} libuv
cd libuv
sh autogen.sh
./configure
make
