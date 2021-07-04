@echo off

if "%1" equ "" (
    echo "Error: invalid arguments, provided file and line open_code {file} {line}"
)

if "%2" equ "" (
    echo "Error: invalid arguments, provided file and line open_code {file} {line}"
)

code --goto "%1:%2"

if %errorlevel% neq 0 (
    "%localappdata%\Programs\Microsoft VS Code\Code.exe" --goto "%1:%2"
)

if %errorlevel% neq 0 (
   %VS_CODE_BIN% --goto "%1:%2"
)

if %errorlevel% neq 0 (
    echo "Error: VsCode not found on the system, try to add it to the path or set VS_CODE_BIN variable"
)