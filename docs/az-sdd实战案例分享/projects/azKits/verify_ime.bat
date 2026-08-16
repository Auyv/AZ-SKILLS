@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0dist\azkits.exe" (
    echo ERROR: dist\azkits.exe was not found. Run build_azKits.bat first.
    exit /b 1
)
chcp 65001 >nul
echo Starting azKits IME prototype...
echo Entry: examples\ime-toolbox.yaml
"%~dp0dist\azkits.exe" --config "%~dp0examples\ime-toolbox.yaml"
set RESULT=%errorlevel%
echo azKits exited with code %RESULT%.
exit /b %RESULT%
