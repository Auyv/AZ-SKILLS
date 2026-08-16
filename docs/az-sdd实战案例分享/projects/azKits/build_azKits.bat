@echo off
setlocal
cd /d "%~dp0"
where go.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: go.exe was not found.
    exit /b 1
)
go test ./...
if errorlevel 1 exit /b %errorlevel%
if not exist dist mkdir dist
set CGO_ENABLED=0
go build -trimpath -ldflags="-s -w" -o dist\azkits.exe ./cmd/azkits
if errorlevel 1 exit /b %errorlevel%
for %%F in (dist\azkits.exe) do echo Built %%~fF ^(%%~zF bytes^)
exit /b 0
