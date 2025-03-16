#!/bin/bash
make clean
make -j EVALFILE=params.bin PROCESS_NET=true
./engine
# make clean
# make -j EVALFILE=quantised.bin
# ./engine bench