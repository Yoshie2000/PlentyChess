make clean && make EXE=PlentyChess-$args-generic.exe arch=generic
make clean && make EXE=PlentyChess-$args-ssse3.exe arch=ssse3
make clean && make EXE=PlentyChess-$args-fma.exe arch=fma
make clean && make EXE=PlentyChess-$args-avx2.exe arch=avx2
make clean && make EXE=PlentyChess-$args-bmi2.exe arch=avx2 BMI2=true
make clean && make EXE=PlentyChess-$args-avx512.exe arch=avx512 BMI2=true
make clean && make EXE=PlentyChess-$args-avx512vnni.exe arch=avx512vnni BMI2=true