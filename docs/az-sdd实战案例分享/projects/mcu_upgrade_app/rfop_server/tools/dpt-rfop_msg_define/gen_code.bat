::/**
:: * @file        gen_code.bat
:: * @brief       XXXX
:: * @note        XXXX
:: * @author      靳普诏(puzhao.jin@hopechart.com)
:: * @date        2024/06/30
:: * @version     1.0
::
:: * @par         修改日志
:: * <table>
:: * <tr><th>Date         <th>Version     <th>Author      <th> Description
:: * <tr><td>2024/06/30   <td>1.0         <td>靳普诏       <td> 创建初始版本
:: * @copyright   xxxx
:: */

@echo off
:: ============================= 获取环境参数 ================================

set ARGV_LEN=%#
::echo "There are %ARGV_LEN% arguments passed to the gen_code.bat"

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
echo "============= BIGEN: gen_code.bat ============="


call dpt -i "%SCRIPT_DIR%\rfop_msg_define.xlsx" -o "%SCRIPT_DIR%\out\rfop_msg_coder"


echo "============= END: gen_code.bat ============="
echo "gen_code.bat return errorlevel:%errorlevel%"

pause
:: Generated on "2024-06-30 17:55:08" by the tool "gen_hq_file.py >> V20231119" 


