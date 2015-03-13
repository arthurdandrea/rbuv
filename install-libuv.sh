#!/bin/sh
set -ex
LIBUV=0.10.36
wget https://github.com/libuv/libuv/archive/v${LIBUV}.tar.gz --output-document libuv-${LIBUV}.tar.gz
tar -xf libuv-${LIBUV}.tar.gz
mv libuv-${LIBUV} libuv
cd libuv
make
mkdir lib
ln -s ../libuv.so lib/libuv.so
ln -s ../libuv.so lib/libuv.so.0.10
