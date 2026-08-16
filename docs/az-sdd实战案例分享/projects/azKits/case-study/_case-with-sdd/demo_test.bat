@echo off
rem Quick launcher for azKits sample toolbox (examples\toolbox.yaml)
setlocal
cd /d "%~dp0"
if exist "%~dp0dist\azkits.exe" (
    "%~dp0dist\azkits.exe" --config "%~dp0examples\demo.yaml"
    exit /b %errorlevel%
)
where go.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: dist\azkits.exe and go.exe were not found.
    echo Please build first with build_azKits.bat or install Go.
    exit /b 1
)
go run ./cmd/azkits --config "%~dp0examples\demo.yaml"
exit /b %errorlevel%
