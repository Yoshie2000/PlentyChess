@echo off
setlocal
set COMPILER=%1
set FLAG=%2

%COMPILER% -march=native -dM -E - <nul > temp_output.txt 2>&1
findstr /C:"%FLAG%" temp_output.txt > nul
if %ERRORLEVEL% equ 0 (
    echo 1
) else (
    echo 0
)

del temp_output.txt
endlocal