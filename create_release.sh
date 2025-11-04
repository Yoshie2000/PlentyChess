make clean && make EXE=PlentyChess-$1-linux-generic arch=generic -j
make clean && make EXE=PlentyChess-$1-linux-ssse3 arch=ssse3 -j
make clean && make EXE=PlentyChess-$1-linux-fma arch=fma -j
make clean && make EXE=PlentyChess-$1-linux-avx2 arch=avx2 -j profile-build
make clean && make EXE=PlentyChess-$1-linux-bmi2 arch=bmi2 -j profile-build
make clean && make EXE=PlentyChess-$1-linux-avx512 arch=avx512 -j profile-build
make clean && make EXE=PlentyChess-$1-linux-avx512vnni arch=avx512vnni -j profile-build