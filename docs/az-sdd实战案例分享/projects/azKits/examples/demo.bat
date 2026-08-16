@echo off
echo BAT demo started. Argument: %~1
ping 127.0.0.1 -n 3 >nul
echo BAT demo completed.
