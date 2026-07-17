param(
    [string]$Workspace = "C:\HQ_Workspace\YLB04_A6",
    [string]$AppName = "",
    [switch]$Execute,
    [string]$ProductName = "YLB04_A6",
    [string]$BuildModel = "ag35_cetda",
    [string]$ProjectName = "",
    [string]$ContainerName = "YLB04_A6",
    [string]$ContainerId = "",
    [string]$MpuRelativePath = "",
    [string]$ContainerWorkDir = "",
    [switch]$Confirmed
)

$ErrorActionPreference = "Stop"

function Fail($Code, $Message) {
    Write-Error "$Code`: $Message"
    exit 1
}

if ([string]::IsNullOrWhiteSpace($AppName)) {
    Fail "ERR_APP_NAME_EMPTY" "AppName is required."
}

if ([string]::IsNullOrWhiteSpace($ProjectName)) {
    $ProjectName = $AppName
}

if ($Execute) {
    if (-not $Confirmed) {
        Fail "ERR_CONFIRMATION_REQUIRED" "Execution requires prior user confirmation of ContainerId, BuildModel, ProductName, and ProjectName. ProjectName defaults to AppName when omitted. Pass -Confirmed after confirmation."
    }
    if ([string]::IsNullOrWhiteSpace($ContainerId)) {
        Fail "ERR_DOCKER_ID_EMPTY" "ContainerId is required when Execute is used."
    }
    if ([string]::IsNullOrWhiteSpace($BuildModel)) {
        Fail "ERR_BUILD_MODEL_EMPTY" "BuildModel is required when Execute is used."
    }
    if ([string]::IsNullOrWhiteSpace($ProductName)) {
        Fail "ERR_PRODUCT_NAME_EMPTY" "ProductName is required when Execute is used."
    }
}

if (-not (Test-Path -LiteralPath $Workspace -PathType Container)) {
    Fail "ERR_WORKSPACE_NOT_FOUND" "Workspace not found: $Workspace"
}

$mpuRelativeCandidates = @()
if (-not [string]::IsNullOrWhiteSpace($MpuRelativePath)) {
    if ([System.IO.Path]::IsPathRooted($MpuRelativePath)) {
        Fail "ERR_MPU_PATH_INVALID" "MpuRelativePath must be relative to Workspace: $MpuRelativePath"
    }
    $mpuRelativeCandidates += $MpuRelativePath.Trim().TrimStart([char[]]".\/")
} else {
    # New repositories use code\task-mpu; retain the legacy layout as a fallback.
    $mpuRelativeCandidates += "code\task-mpu"
    $mpuRelativeCandidates += "code\task-jpz_dev\mpu"
}

$mpuDir = $null
$selectedMpuRelativePath = $null
$existingMpuDirs = New-Object System.Collections.Generic.List[string]
$checkedBuildScripts = New-Object System.Collections.Generic.List[string]
foreach ($candidate in $mpuRelativeCandidates) {
    $candidateMpuDir = Join-Path $Workspace $candidate
    $candidateBuildScript = Join-Path $candidateMpuDir "app\$AppName\build\build.sh"
    $checkedBuildScripts.Add($candidateBuildScript)
    if (Test-Path -LiteralPath $candidateMpuDir -PathType Container) {
        $existingMpuDirs.Add($candidateMpuDir)
        if (Test-Path -LiteralPath $candidateBuildScript -PathType Leaf) {
            $mpuDir = $candidateMpuDir
            $selectedMpuRelativePath = $candidate
            break
        }
    }
}

if ($null -eq $mpuDir) {
    if ($existingMpuDirs.Count -eq 0) {
        Fail "ERR_MPU_DIR_NOT_FOUND" "MPU directory not found. Checked: $($mpuRelativeCandidates -join '; ')"
    }
    Fail "ERR_APP_BUILD_SCRIPT_MISSING" "Missing app build script. Checked: $($checkedBuildScripts -join '; ')"
}

if ([string]::IsNullOrWhiteSpace($ContainerWorkDir)) {
    $containerRelativePath = $selectedMpuRelativePath -replace '\\', '/'
    $ContainerWorkDir = "/home/sei/$containerRelativePath"
}

$safeAppName = $AppName -replace '[^A-Za-z0-9_.-]', '_'
$linuxScriptPath = Join-Path $mpuDir "az_build_linux_app_$safeAppName.sh"
$batScriptPath = Join-Path $mpuDir "az_build_linux_$safeAppName.bat"
$logDir = Join-Path $Workspace "ai_tmp\mpu_debug_build"
$dockerConfigDir = Join-Path $Workspace "ai_tmp\docker_config"

$linuxScript = @"
#!/bin/bash

SCRIPT_DIR=`$(dirname "`$(readlink -f "`$0")")

_CHECK_ERRORLEVEL() {
    local errorlevel=`$1
    local prompt="`${2:-}"
    local force_exit="`${3:-false}"
    force_exit=`$(echo "`$force_exit" | tr '[:upper:]' '[:lower:]')

    if [ `$errorlevel -eq 0 ]; then
        echo "   `${prompt} succeeded"
    else
        echo "ERROR: `${prompt} failed with errorlevel `$errorlevel"
        if [ "`$force_exit" = "true" ]; then
            exit `$errorlevel
        fi
    fi
    return `$errorlevel
}

if [ -f /home/ql-ol-sdk/utils/gdb/usr/bin/gdbserver ]; then
    cp /home/ql-ol-sdk/utils/gdb/usr/bin/gdbserver "`$SCRIPT_DIR" >/dev/null 2>&1 || true
fi

echo ============Jpz: mpu app build: $AppName============

export G_PROJECT_NAME=$ProjectName
export G_PRODUCT_NAME=$ProductName
export G_BUILD_MODEL=$BuildModel
export BUILD_PROJECT_NAME=`$G_PROJECT_NAME
export BUILD_PRODUCT_NAME=`$G_PRODUCT_NAME
export BUILD_PLATFORM_NAME=`$G_BUILD_MODEL
echo G_PROJECT_NAME=`$G_PROJECT_NAME
echo G_PRODUCT_NAME=`$G_PRODUCT_NAME
echo G_BUILD_MODEL=`$G_BUILD_MODEL

STAGING_DIR=`${PROJECT_DIR}/temp
export STAGING_DIR
echo STAGING_DIR=`$STAGING_DIR

APP_NAME=$AppName
"`$SCRIPT_DIR/app/`$APP_NAME/build/build.sh"
_CHECK_ERRORLEVEL `$? "`$APP_NAME" "true"

BUILD_TIME=`$(date "+%Y-%m-%d %H:%M:%S")
echo ============ Build Time:`${BUILD_TIME} ============
"@

$batScript = @"
@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "DOCKER_CONTAINER_NAME=$ContainerName"
set "DOCKER_CONTAINER_ID=$ContainerId"
set "CONTAINER_WORK_DIR=$ContainerWorkDir"
set "LINUX_BUILD_SCRIPT=./az_build_linux_app_$safeAppName.sh"
set "DOCKER_CONFIG=$dockerConfigDir"

echo ============= Build MPU app: $AppName =============
echo.

where docker >nul 2>nul
if !errorlevel! neq 0 (
    echo ERR_DOCKER_NOT_FOUND: docker command not found or Docker Desktop is not running.
    set "BUILD_RESULT=1"
    goto :fail
)

for /f %%i in ('docker ps -a -q -f "name=^%DOCKER_CONTAINER_NAME%$"') do set "DOCKER_CONTAINER_ID=%%i"

if "%DOCKER_CONTAINER_ID%"=="" (
    echo ERR_CONTAINER_NOT_FOUND: %DOCKER_CONTAINER_NAME%
    set "BUILD_RESULT=1"
    goto :fail
)

docker exec %DOCKER_CONTAINER_ID% /bin/true >nul 2>nul
if !errorlevel! neq 0 (
    echo [INFO] Start Docker container %DOCKER_CONTAINER_ID% ...
    docker start %DOCKER_CONTAINER_ID%
    set "START_RESULT=!errorlevel!"
    if !START_RESULT! neq 0 (
        echo ERR_CONTAINER_START_FAILED: %DOCKER_CONTAINER_ID%
        set "BUILD_RESULT=!START_RESULT!"
        goto :fail
    )
    timeout /t 3 /nobreak >nul
    docker exec %DOCKER_CONTAINER_ID% /bin/true >nul 2>nul
    if !errorlevel! neq 0 (
        echo ERR_CONTAINER_START_FAILED: %DOCKER_CONTAINER_ID%
        call :dump_container_diag
        set "BUILD_RESULT=1"
        goto :fail
    )
)

docker exec -i %DOCKER_CONTAINER_ID% /bin/sh -c "cd %CONTAINER_WORK_DIR% && chmod +x %LINUX_BUILD_SCRIPT% && %LINUX_BUILD_SCRIPT%"
set "BUILD_RESULT=!errorlevel!"
if "!BUILD_RESULT!"=="0" (
    rem keep success
) else (
    goto :fail
)

echo.
if !BUILD_RESULT! equ 0 (
    echo ============= Build succeeded: $AppName =============
) else (
    echo ============= Build failed: $AppName return !BUILD_RESULT! =============
)

exit /b !BUILD_RESULT!

:fail
echo ============= Build failed: $AppName return !BUILD_RESULT! =============
exit /b !BUILD_RESULT!

:dump_container_diag
echo ----- docker ps status -----
docker ps -a --filter "id=%DOCKER_CONTAINER_ID%" --format "table {{.ID}}\t{{.Names}}\t{{.Status}}\t{{.Ports}}"
echo ----- docker inspect status -----
docker inspect --format="Name={{.Name}} Status={{.State.Status}} Running={{.State.Running}} ExitCode={{.State.ExitCode}} Error={{.State.Error}} FinishedAt={{.State.FinishedAt}}" %DOCKER_CONTAINER_ID%
echo ----- docker logs tail -----
docker logs --tail 80 %DOCKER_CONTAINER_ID%
echo ----- end docker diag -----
exit /b 0
"@

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

# .sh 保持 LF（Linux 容器需要 LF 换行）
[System.IO.File]::WriteAllText($linuxScriptPath, $linuxScript, $utf8NoBom)

# .bat 必须 CRLF（Windows cmd 解析 .bat 需要 CRLF；WriteAllText 默认写 LF 会导致中文 Windows / cmd 解析异常）
$batBytes = [System.Text.Encoding]::UTF8.GetBytes($batScript)
$batOut = New-Object System.Collections.Generic.List[byte]
foreach ($b in $batBytes) {
    if ($b -eq 0x0A) {
        $batOut.Add(0x0D); $batOut.Add(0x0A)
    } elseif ($b -ne 0x0D) {
        $batOut.Add($b)
    }
}
[System.IO.File]::WriteAllBytes($batScriptPath, $batOut.ToArray())

Write-Host "Generated Windows script: $batScriptPath"
Write-Host "Generated Linux script:   $linuxScriptPath"
Write-Host "Detected MPU root:        $mpuDir"
Write-Host "Container work directory: $ContainerWorkDir"

if ($Execute) {
    Write-Host "Confirmed build parameters:"
    Write-Host "  Docker Id:    $ContainerId"
    Write-Host "  Build Model:  $BuildModel"
    Write-Host "  Product Name: $ProductName"
    Write-Host "  Project Name: $ProjectName"
    New-Item -ItemType Directory -Force -Path $dockerConfigDir | Out-Null
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $logPath = Join-Path $logDir "$safeAppName`_$timestamp.log"
    Write-Host "Build log: $logPath"
    & cmd.exe /c "call `"$batScriptPath`" > `"$logPath`" 2>&1"
    $exitCode = $LASTEXITCODE
    Get-Content -LiteralPath $logPath -Tail 80
    Write-Host "ExitCode: $exitCode"
    exit $exitCode
}
