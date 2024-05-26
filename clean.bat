@echo off

set OPERATION=%~1

if "%OPERATION%" == "pgo" (
    rmdir /s /q pgo
) else (
    del /s /q src\*.o
    del /q *~
    del /s /q engine
)