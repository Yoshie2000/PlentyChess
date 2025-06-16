make clean && make profile-build CXX=g++ EXE=PlentyChess-$args-windows-generic.exe arch=generic -j
make clean && make profile-build CXX=g++ EXE=PlentyChess-$args-windows-ssse3.exe arch=ssse3 -j
make clean && make profile-build CXX=g++ EXE=PlentyChess-$args-windows-fma.exe arch=fma -j
make clean && make profile-build CXX=g++ EXE=PlentyChess-$args-windows-avx2.exe arch=avx2 -j profile-build
make clean && make profile-build CXX=g++ EXE=PlentyChess-$args-windows-bmi2.exe arch=avx2 BMI2=true -j profile-build
make clean && make profile-build CXX=g++ EXE=PlentyChess-$args-windows-avx512.exe arch=avx512 BMI2=true -j profile-build
make clean && make profile-build CXX=g++ EXE=PlentyChess-$args-windows-avx512vnni.exe arch=avx512vnni BMI2=true -j profile-build