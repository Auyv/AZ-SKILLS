@echo off
chcp 65001 >nul
REM ==========================================
REM   Docker 镜像构建脚本
REM   使用说明：修改下面的变量值后运行
REM ==========================================
echo.
echo ==========================================
echo   Docker 镜像构建脚本
echo ==========================================
echo.

REM 设置镜像名称和标签
set IMAGE_NAME={{镜像仓库}}/{{项目名}}/{{功能}}/{{平台}}/{{内容}}
set IMAGE_TAG={{版本号}}

REM 设置 Docker 构建上下文路径（可选，默认当前目录）
set BUILD_CONTEXT=.

REM 显示配置信息
echo 配置信息：
echo   镜像名称: %IMAGE_NAME%
echo   镜像标签: %IMAGE_TAG%
echo   构建上下文: %BUILD_CONTEXT%
echo.

REM 询问是否继续
set /p CONFIRM="确认构建？(Y/N): "
if /i not "%CONFIRM%"=="Y" (
    echo 取消构建。
    pause
    exit /b 0
)

echo.
echo [%date% %time%] 开始构建镜像...
docker build -t %IMAGE_NAME%:%IMAGE_TAG% %BUILD_CONTEXT%

REM 检查构建结果
if %errorlevel% equ 0 (
    echo.
    echo ==========================================
    echo   构建成功！
    echo   镜像: %IMAGE_NAME%:%IMAGE_TAG%
    echo ==========================================
    
    REM 显示镜像信息
    echo.
    echo 镜像信息：
    docker images %IMAGE_NAME% --format "table {{.Repository}}:{{.Tag}}  {{.Size}}  {{.CreatedAt}}"
) else (
    echo.
    echo ==========================================
    echo   构建失败！
    echo ==========================================
    exit /b 1
)

echo.
REM 可选：推送到仓库
set /p PUSH="是否推送到仓库？(Y/N): "
if /i "%PUSH%"=="Y" (
    echo [%date% %time%] 推送镜像到仓库...
    docker push %IMAGE_NAME%:%IMAGE_TAG%
    if %errorlevel% equ 0 (
        echo.
        echo 推送成功！
    ) else (
        echo.
        echo 推送失败！
    )
)

echo.
pause
