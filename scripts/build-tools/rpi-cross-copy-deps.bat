@echo off

if "%1"=="" (
    echo Error: missing root directory of rpi-toolchain: rpi-cross-copy-deps {rpi-root-dir}
    exit /b -1
)

if not exist %1 (
    echo Error: rpi toolchain direcory '%1' does not exist
    exit /b -1
)

powershell -Command "Expand-Archive -Path 'rpi-buster-asound-2.0.0.zip' -DestinationPath '%1\arm-linux-gnueabihf\sysroot\usr'"
