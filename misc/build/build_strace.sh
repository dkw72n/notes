#!/bin/sh
git clone https://github.com/strace/strace
cd strace
./bootstrap
export LDFLAGS='-static -pthread'

# --enable-mpers=no: for `configure: error: Cannot enable m32 personality support` 
./configure --enable-mpers=no 

make -j4
