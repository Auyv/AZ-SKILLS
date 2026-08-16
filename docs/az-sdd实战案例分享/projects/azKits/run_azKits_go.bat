@echo off
setlocal
cd /d "%~dp0"
if exist "%~dp0dist\azkits.exe" (
    "%~dp0dist\azkits.exe" --config "%~dp0examples\toolbox.yaml"
    exit /b %errorlevel%
)
where go.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: dist\azkits.exe and go.exe were not found.
    exit /b 1
)
go run ./cmd/azkits --config "%~dp0examples\toolbox.yaml"
exit /b %errorlevel%
