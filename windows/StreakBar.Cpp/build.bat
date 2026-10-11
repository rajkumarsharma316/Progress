@echo off
echo Compiling StreakBar with Embedded Icon and Local Wi-Fi Sync...
windres resource.rc -O coff -o resource.res
g++ -O3 -s -mwindows -std=c++17 main.cpp resource.res -o StreakBar.exe -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -lws2_32 -static -static-libgcc -static-libstdc++
if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] StreakBar.exe compiled successfully with application icon!
    echo Standalone native binary ready.
) else (
    echo.
    echo [ERROR] Compilation failed.
)
pause
