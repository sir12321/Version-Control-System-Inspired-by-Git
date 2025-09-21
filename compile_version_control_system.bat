@echo off
REM Compile long_assignment_1.cpp using g++
g++ -g long_assignment_1.cpp -o long_assignment_1.exe
if %errorlevel% neq 0 (
    echo Compilation failed.
    exit /b %errorlevel%
) else (
    echo Compilation successful. Output: long_assignment_1.exe
)
