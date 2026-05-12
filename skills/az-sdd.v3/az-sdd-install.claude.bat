@echo off
chcp 65001 >nul
echo ========================================
echo az-sdd 全套技能安装到 Claude 全局目录
echo ========================================
echo.

python "%~dp0az-sdd-install.py" --claude

echo.
pause
