@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (echo Python launcher "py" was not found. Install Python 3.10+ from python.org. & pause & exit /b 1)
py -3 -c "import serial" >nul 2>nul
if errorlevel 1 (echo Installing PySerial for the current user... & py -3 -m pip install --user pyserial)
echo Close Arduino Serial Monitor and any other app using the XIAO first.
echo.
set /p PORT=Enter the XIAO COM port (for example COM9): 
if "%PORT%"=="" set PORT=COM9
py -3 tinychat.py %PORT%
pause
