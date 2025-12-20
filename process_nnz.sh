#!/bin/bash
make clean
make -j EVALFILE=raw.bin PROCESS_NET=true
./engine bench
make clean
make -j EVALFILE=quantised.bin
./engine bench