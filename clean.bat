@echo off
setlocal
cd /d "%~dp0"

echo Removing build output...
if exist "bin" (
    rmdir /s /q "bin"
    echo   deleted bin\
)
if exist "build" (
    rmdir /s /q "build"
    echo   deleted build\
)
echo Clean complete. Source files and data\users.txt were kept.

endlocal
pause
