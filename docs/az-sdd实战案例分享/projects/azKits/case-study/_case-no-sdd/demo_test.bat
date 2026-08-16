@echo off
rem azKits demo quick launcher
setlocal
cd /d "%~dp0"

if not exist azkits.exe (
    echo [build] azkits.exe not found, building...
    go build -o azkits.exe . || goto :fail
)

echo [run] azkits demo entry: examples\demo.yaml
echo [hint] type "/" and filter by alias/name; Ctrl+F favorite; Ctrl+C quit
azkits.exe examples\demo.yaml
set EC=%ERRORLEVEL%
if "%EC%"=="0" goto :end

:fail
echo.
echo [error] exit code %EC%
pause

:end
endlocal
exit /b %EC%
