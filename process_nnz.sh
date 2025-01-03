#!/bin/bash
make clean
make -j EVALFILE=params.bin PROCESS_NET=true NETFILES=params.bin,params2.bin
./engine bench
make clean
make -j EVALFILE=quantised.bin
./engine bench