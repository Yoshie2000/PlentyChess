#!/bin/bash
make clean
make
./process_net
cd ..
make clean
make nopgo EVALFILE=network.bin PROCESS_NET=true
./engine bench
make clean
make nopgo EVALFILE=network.bin
./engine bench