make clean && make EXE=PlentyChess-$1-generic arch=generic
make clean && make EXE=PlentyChess-$1-ssse3 arch=ssse3
make clean && make EXE=PlentyChess-$1-fma arch=fma
make clean && make EXE=PlentyChess-$1-avx2 arch=avx2
make clean && make EXE=PlentyChess-$1-bmi2 arch=avx2 BMI2=true
make clean && make EXE=PlentyChess-$1-avx512 arch=avx512 BMI2=true
make clean && make EXE=PlentyChess-$1-avx512vnni arch=avx512vnni BMI2=true