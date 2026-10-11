@echo off
echo Compiling StreakBar (Pure C++ / Win32 with Local Wi-Fi Sync)...
g++ -O3 -s -mwindows -std=c++17 main.cpp -o StreakBar.exe -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -lws2_32 -static -static-libgcc -static-libstdc++
if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] StreakBar.exe compiled successfully!
    echo Standalone native binary with Local Wi-Fi Sync ready.
) else (
    echo.
    echo [ERROR] Compilation failed.
)
pause
