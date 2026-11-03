@echo off
setlocal
cd /d "%~dp0"

where wsl >nul 2>nul
if errorlevel 1 (
    echo WSL is not installed. Open PowerShell as admin and run: wsl --install
    goto end
)

echo Building Secure DDS Pong in WSL...
echo.
wsl make
if errorlevel 1 (
    echo.
    echo Build FAILED. If a library is missing, run this inside WSL:
    echo   sudo apt install cyclonedds-dev cyclonedds-tools libglfw3-dev libgl1-mesa-dev libsodium-dev pkgconf make
) else (
    echo.
    echo Build OK. Programs are in the bin folder.
    echo In WSL run: ./bin/server   then in two more terminals: ./bin/client player1   and   ./bin/client player2
)

:end
endlocal
pause
