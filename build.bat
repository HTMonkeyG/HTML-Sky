@echo off
cd %~dp0
cmake -S . -B build -G Ninja
cmake --build build
