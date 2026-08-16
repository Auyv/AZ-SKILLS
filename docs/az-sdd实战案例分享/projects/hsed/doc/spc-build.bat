:: file        build.bat
:: brief       XXXX
:: note        XXXX
:: author      靳普诏(puzhao.jin@hopechart.com)
:: date        2026/02/28
:: version     1.0

@echo off
setlocal EnableDelayedExpansion
:: ============================= 获取环境参数 ================================

set ARGV_LEN=%#
::echo "There are %ARGV_LEN% arguments passed to the build.bat"

set i=1
:show_argument
if %i% gtr %ARGV_LEN% goto end_show_argument

set arg=!%i%!
::echo "Argument %i%: %arg%"

set /a i+=1
goto show_argument

:end_show_argument
::echo "All arguments printed"

:: 脚本所在路径
set SCRIPT_DIR=%~dp0
echo "SCRIPT_DIR=%SCRIPT_DIR%"
:: 当前工作路径
set CURRENT_DIR=%cd%
echo "CURRENT_DIR=%CURRENT_DIR%"    

:: ============================= 实际调用逻辑 ================================
pushd %SCRIPT_DIR%
echo "============= BIGEN: build.bat ============="

set SPC_BUILDER="C:\HQ_Workspace\HQ_Tools\SpcBuilder\spc_builder_ver-tool\spc_builder_ver-1.11.exe"
set OUT_DIR="C:\HQ_Workspace\YLB04_A6\code\task-jpz_dev\mpu\app\hsed\doc\hsed-spc_out"
set IN_XLSX="C:\HQ_Workspace\YLB04_A6\code\task-jpz_dev\mpu\app\hsed\doc\hsed.xlsx"


if exist %OUT_DIR% (
    rmdir /s /q %OUT_DIR%
)
mkdir %OUT_DIR%

::call %SPC_BUILDER% -i can_server-spc_def.xlsx -o can_server-spc_out



call %SPC_BUILDER% -i %IN_XLSX% -o %OUT_DIR%







echo "============= END: build.bat ============="
echo "build.bat return errorlevel:%errorlevel%"
popd
pause
endlocal


