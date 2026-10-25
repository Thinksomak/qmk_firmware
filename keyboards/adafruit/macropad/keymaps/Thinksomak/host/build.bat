@echo off
rem Builds the agent as one .exe with PyInstaller, in a local .venv.
rem   build.bat            dist\macropad-sysinfo.exe, no window (for every day)
rem   build.bat --console  dist\macropad-sysinfo-console.exe, logs in a window (for debugging)
setlocal
cd /d "%~dp0"

set "NAME=macropad-sysinfo"
set "MODE=--noconsole"
if /i "%~1"=="--console" (
    set "NAME=macropad-sysinfo-console"
    set "MODE=--console"
)
if /i "%~1"=="console" (
    set "NAME=macropad-sysinfo-console"
    set "MODE=--console"
)

if not exist .venv\Scripts\python.exe (
    echo Creating .venv ...
    py -3 -m venv .venv 2>nul || python -m venv .venv || goto :fail
)
.venv\Scripts\python -m pip install --quiet --upgrade pip || goto :fail
.venv\Scripts\python -m pip install --quiet -r requirements.txt pyinstaller || goto :fail
.venv\Scripts\python -m PyInstaller --noconfirm --clean --onefile %MODE% --name %NAME% main.py || goto :fail
copy /y macropad_sysinfo.ini dist\ >nul
echo.
echo Built dist\%NAME%.exe (settings: dist\macropad_sysinfo.ini)
exit /b 0

:fail
echo Build failed.
exit /b 1
