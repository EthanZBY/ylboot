#!/bin/bash

CMD=`realpath $0`
COMMON_DIR=`dirname $CMD`
TOP_DIR=$(realpath $COMMON_DIR/)
export KCFLAGS=-fno-stack-protector
./make.sh rk3588-yourland CROSS_COMPILE=aarch64-linux-gnu- --idblock
if [ "$?" == "0" ]; then
    mkdir -p out
    cp -f rk3588_spl_loader_*.bin out/3588_loader.bin
    cp -f uboot.img out/
fi
