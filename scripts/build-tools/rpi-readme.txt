# How to cross-compile for RPI

- Download pre-built toolchain from https://gnutoolchains.com/raspberry/ and extract it somewhere
- There is an extra alsa sound dependency (asound) that you should copy to your sysroot, run rpi-cross-copy-deps.bat to copy prebuilt binaries and includes to target toolchain sysroot
- Use these settings for cmake, I put vscode cmake-tools settings just for vscode convenience:

```
"cmake.buildDirectory": "${workspaceFolder}/build-rpi",
"cmake.configureArgs": ["-DCMAKE_TOOLCHAIN_FILE=${workspaceFolder}/cmake/rpi.cmake", "-DRPI_SYSGCC_ROOT={rpi_toolchain_dir}"]
```


