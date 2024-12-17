#!/bin/bash
make clean
make nopgo EVALFILE=params.bin PROCESS_NET=true
./engine bench
make clean
make nopgo EVALFILE=params.bin
./engine bench