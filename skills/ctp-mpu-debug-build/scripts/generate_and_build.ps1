param(
    [string]$Workspace = "C:\HQ_Workspace\YLB04_A6",
    [string]$AppName = "",
    [switch]$Execute,
    [string]$ProductName = "YLB04_A6",
    [string]$BuildModel = "ag35_cetda",
    [string]$ProjectName = "",
    [string]$ContainerId = "",
    [string]$MpuRelativePath = "",
    [string]$ContainerWorkDir = "",
    [string]$StagingDir = "/home/sdk/ql-sysroots",
    [ValidateSet("ON", "OFF")]
    [string]$CanServerPerfTestEnable = "OFF",
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
$batScriptPath = Join-Path $mpuDir "az_build_linux_$safeAppName.bat"
$logDir = Join-Path $Workspace "ai_tmp\mpu_debug_build"

$batScript = @"
@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Fixed Docker target; no container-name lookup or Docker CLI config directory.
set "DOCKER_CONTAINER_ID=$ContainerId"
set "CONTAINER_WORK_DIR=$ContainerWorkDir"
set "G_PROJECT_NAME=$ProjectName"
set "G_PRODUCT_NAME=$ProductName"
set "G_BUILD_MODEL=$BuildModel"
set "CAN_SERVER_PERF_TEST_ENABLE=$CanServerPerfTestEnable"
set "STAGING_DIR=$StagingDir"
set "BUILD_LOG_DIR=$logDir"

if not exist "%BUILD_LOG_DIR%" mkdir "%BUILD_LOG_DIR%"
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "BUILD_TIMESTAMP=%%i"
if not defined BUILD_TIMESTAMP set "BUILD_TIMESTAMP=unknown"
set "BUILD_LOG_PATH=%BUILD_LOG_DIR%\${safeAppName}_%BUILD_TIMESTAMP%.log"
echo MPU app: $AppName > "%BUILD_LOG_PATH%"
echo Container ID: %DOCKER_CONTAINER_ID% >> "%BUILD_LOG_PATH%"
echo Product: %G_PRODUCT_NAME%  Model: %G_BUILD_MODEL% >> "%BUILD_LOG_PATH%"

echo ============= Build MPU app: $AppName =============
echo Container ID: %DOCKER_CONTAINER_ID%
echo Product: %G_PRODUCT_NAME%  Model: %G_BUILD_MODEL%
echo Log: %BUILD_LOG_PATH%
echo.

where docker >nul 2>nul
if errorlevel 1 (
    echo ERR_DOCKER_NOT_FOUND: docker command not found or Docker Desktop is not running.
    set "BUILD_RESULT=1"
    goto :fail
)

docker inspect --type container "%DOCKER_CONTAINER_ID%" >nul 2>nul
if errorlevel 1 (
    echo ERR_CONTAINER_NOT_FOUND: %DOCKER_CONTAINER_ID%
    set "BUILD_RESULT=1"
    goto :fail
)

set "CONTAINER_RUNNING="
for /f "delims=" %%i in ('docker inspect --type container -f "{{.State.Running}}" "%DOCKER_CONTAINER_ID%"') do set "CONTAINER_RUNNING=%%i"
if /i not "!CONTAINER_RUNNING!"=="true" (
    echo [INFO] Starting container %DOCKER_CONTAINER_ID% ...
    docker start "%DOCKER_CONTAINER_ID%" >nul
    if errorlevel 1 (
        set "BUILD_RESULT=!errorlevel!"
        goto :fail
    )
)

set /a WAIT_ATTEMPTS=0
:wait_container
    docker exec "%DOCKER_CONTAINER_ID%" /bin/true >nul 2>nul
    if !errorlevel! equ 0 goto :container_ready
    set /a WAIT_ATTEMPTS+=1
    if !WAIT_ATTEMPTS! geq 15 (
        echo ERR_CONTAINER_NOT_READY: %DOCKER_CONTAINER_ID%
        set "BUILD_RESULT=1"
        goto :fail
    )
    timeout /t 1 /nobreak >nul
goto :wait_container

:container_ready
echo Running MPU build inside container...
docker exec "%DOCKER_CONTAINER_ID%" /bin/bash -lc "set -e; cd '%CONTAINER_WORK_DIR%'; if [ -f /home/ql-ol-sdk/utils/gdb/usr/bin/gdbserver ]; then cp /home/ql-ol-sdk/utils/gdb/usr/bin/gdbserver . >/dev/null 2>&1 || true; fi; export G_PROJECT_NAME=%G_PROJECT_NAME% G_PRODUCT_NAME=%G_PRODUCT_NAME% G_BUILD_MODEL=%G_BUILD_MODEL% BUILD_PROJECT_NAME=%G_PROJECT_NAME% BUILD_PRODUCT_NAME=%G_PRODUCT_NAME% BUILD_PLATFORM_NAME=%G_BUILD_MODEL% CAN_SERVER_PERF_TEST_ENABLE=%CAN_SERVER_PERF_TEST_ENABLE% STAGING_DIR=%STAGING_DIR%; bash app/$AppName/build/build.sh Debug" >>"%BUILD_LOG_PATH%" 2>&1
set "BUILD_RESULT=!errorlevel!"
type "%BUILD_LOG_PATH%"
if not "!BUILD_RESULT!"=="0" goto :fail

echo.
echo ============= Build succeeded: $AppName =============
echo Artifact: $selectedMpuRelativePath/build_output/app/debug/$AppName
echo Log: %BUILD_LOG_PATH%
exit /b 0

:fail
echo.
echo ============= Build failed: $AppName return !BUILD_RESULT! =============
echo Log: %BUILD_LOG_PATH%
docker ps -a --filter "id=%DOCKER_CONTAINER_ID%" --format "table {{.ID}}\t{{.Names}}\t{{.Status}}" 2>nul
docker logs --tail 40 "%DOCKER_CONTAINER_ID%" 2>nul
exit /b !BUILD_RESULT!
"@

[System.IO.File]::WriteAllText($batScriptPath, ($batScript -replace "`r?`n", "`r`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Generated Windows script: $batScriptPath"
Write-Host "Detected MPU root:        $mpuDir"
Write-Host "Container work directory: $ContainerWorkDir"
if ($Execute) {
    Write-Host "Confirmed build parameters:"
    Write-Host "  Docker Id:    $ContainerId"
    Write-Host "  Build Model:  $BuildModel"
    Write-Host "  Product Name: $ProductName"
    Write-Host "  Project Name: $ProjectName"
    & cmd.exe /c "call `"$batScriptPath`""
    $exitCode = $LASTEXITCODE
    $latestLog = Get-ChildItem -LiteralPath $logDir -Filter "$safeAppName`_*.log" -File |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($null -ne $latestLog) {
        Write-Host "Build log: $($latestLog.FullName)"
        Get-Content -LiteralPath $latestLog.FullName -Tail 80
    }
    Write-Host "ExitCode: $exitCode"
    exit $exitCode
}
