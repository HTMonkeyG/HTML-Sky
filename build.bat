@echo off
setlocal
cd /d "%~dp0"

if "%~1"=="clear" (
  rmdir /s /q dist
  rmdir /s /q build
) else (
  cmake -S . -B build -G Ninja
  cmake --build build
)
