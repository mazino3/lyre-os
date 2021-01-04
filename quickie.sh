#!/usr/bin/env bash

rm -f lyre.hdd && ( cd kernel/ && make clean ) && MAKEFLAGS="DBGOUT=qemu -j8" ./bootstrap.sh build/ && ./run.sh
