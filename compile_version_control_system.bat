@echo off
REM Compile version_control_system.cpp using g++
g++ -g version_control_system.cpp -o version_control_system.exe
if %errorlevel% neq 0 (
    echo Compilation failed.
    exit /b %errorlevel%
) else (
    echo Compilation successful. Output: version_control_system.exe
)
