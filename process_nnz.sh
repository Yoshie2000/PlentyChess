#!/bin/bash
make clean
make -j EVALFILE=params.bin PROCESS_NET=true
./engine bench
make clean
make -j EVALFILE=compressed.bin
./engine bench