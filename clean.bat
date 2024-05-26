@echo off

set OPERATION=%~1
if "%OPERATION%" == "pgo" (
    rmdir /s /q pgo >nul 2>&1
) else (
    del /s /q src\*.o >nul 2>&1
    del /q *~ >nul 2>&1
    del /s /q engine >nul 2>&1
)