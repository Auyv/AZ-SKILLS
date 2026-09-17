param(
    [Parameter(Mandatory = $true)]
    [string]$Workspace,
    [string]$OutputPath,
    [string]$BuildModel
)

$ErrorActionPreference = 'Stop'
$Workspace = (Resolve-Path -LiteralPath $Workspace).Path

$workspaceItem = Get-Item -LiteralPath $Workspace
$directBuildScript = Join-Path $workspaceItem.FullName 'build\mpu\build.sh'
if (Test-Path -LiteralPath $directBuildScript -PathType Leaf) {
    $projectDir = $workspaceItem
    $buildScript = Get-Item -LiteralPath $directBuildScript
} else {
    $legacyRoot = Join-Path $workspaceItem.FullName 'autobuild\test_mpu'
    if (-not (Test-Path -LiteralPath $legacyRoot -PathType Container)) {
        throw "ERR_BUILD_TREE_AMBIGUOUS: expected '$directBuildScript' or a legacy build tree under '$legacyRoot'"
    }
    $buildScripts = @(Get-ChildItem -LiteralPath $legacyRoot -Recurse -File -Filter 'build.sh' |
        Where-Object { $_.FullName -match '[\\/]build[\\/]mpu[\\/]build\.sh$' })
    if ($buildScripts.Count -ne 1) {
        $candidates = ($buildScripts.FullName -join '; ')
        throw "ERR_BUILD_TREE_AMBIGUOUS: expected one MPU build.sh, found $($buildScripts.Count). Candidates: $candidates"
    }
    $buildScript = $buildScripts[0]
    $projectDir = $buildScript.Directory.Parent.Parent
}

$configPath = $null
$configRoot = $projectDir
while ($null -ne $configRoot) {
    $candidate = Join-Path $configRoot.FullName 'tools\docker_cmd\config.json'
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $configPath = $candidate
        break
    }
    $configRoot = $configRoot.Parent
}
if (-not $configPath) {
    throw "ERR_CONFIG_MISSING: tools\docker_cmd\config.json not found in '$($projectDir.FullName)' or any parent directory"
}

$config = Get-Content -Raw -Encoding UTF8 -LiteralPath $configPath | ConvertFrom-Json
$containers = @($config.containers)
if ($containers.Count -ne 1) {
    throw "ERR_CONFIG_MISSING: expected exactly one container in $configPath, found $($containers.Count)"
}
$container = $containers[0]
$containerName = [string]$container.name
$containerImage = [string]$container.image
if (-not $containerImage) {
    throw "ERR_CONFIG_MISSING: container '$containerName' has no image in $configPath"
}

$mountPoint = $null
$volumeHostPath = $null
foreach ($volume in @($container.volumes)) {
    $text = [string]$volume
    $match = [regex]::Match($text, '^(?<host>[A-Za-z]:/[^:]*):(?<guest>/.*)$')
    if ($match.Success) {
        $hostPath = $match.Groups['host'].Value.Replace('/', '\').TrimEnd('\')
        $hostPrefix = $hostPath + '\'
        $containsProject = [string]::Equals($hostPath, $projectDir.FullName.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase) -or
            $projectDir.FullName.StartsWith($hostPrefix, [StringComparison]::OrdinalIgnoreCase)
        if ($containsProject -and ($null -eq $volumeHostPath -or $hostPath.Length -gt $volumeHostPath.Length)) {
            $mountPoint = $match.Groups['guest'].Value.TrimEnd('/')
            $volumeHostPath = $hostPath
        }
    }
}
if (-not $mountPoint) {
    throw "ERR_CONFIG_MISSING: no configured volume contains project '$($projectDir.FullName)' in container '$containerName'"
}

$mpuDir = $buildScript.Directory
$buildDir = $mpuDir.Parent
$projectName = $projectDir.Name
$buildRoot = $projectDir.Parent

$projectConfigDir = Join-Path $projectDir.FullName 'config'
if (-not (Test-Path -LiteralPath $projectConfigDir -PathType Container)) {
    throw "ERR_PRODUCT_CONFIG_MISSING: project config directory not found: $projectConfigDir"
}
$listEnvFiles = @(Get-ChildItem -LiteralPath $projectConfigDir -Recurse -File -Filter 'list.env')
if ($listEnvFiles.Count -ne 1) {
    $candidates = ($listEnvFiles.FullName -join '; ')
    throw "ERR_PRODUCT_CONFIG_MISSING: expected one list.env, found $($listEnvFiles.Count). Candidates: $candidates"
}
$listEnv = $listEnvFiles[0]
$productName = $listEnv.Directory.Name
$useExplicitBuildModel = -not [string]::IsNullOrWhiteSpace($BuildModel)
if (-not $BuildModel) {
    $envText = Get-Content -Raw -Encoding UTF8 -LiteralPath $listEnv.FullName
    $modelMatch = [regex]::Match($envText, '(?m)^\s*(?:export\s+)?G_PRODUCT_BUILD_MPU_MODEL\s*=\s*["'']?(?<model>[^"''\s#]+)')
    if (-not $modelMatch.Success) {
        throw "ERR_PRODUCT_CONFIG_MISSING: G_PRODUCT_BUILD_MPU_MODEL not found in $($listEnv.FullName)"
    }
    $BuildModel = $modelMatch.Groups['model'].Value
}

$volumePrefix = $volumeHostPath.TrimEnd('\') + '\'
if (-not $buildRoot.FullName.StartsWith($volumePrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ERR_BUILD_WORKDIR: build root '$($buildRoot.FullName)' is outside configured volume '$volumeHostPath'"
}
$relativeBuildRoot = $buildRoot.FullName.Substring($volumePrefix.Length).Replace('\', '/')
$dockerBuildDir = if ($relativeBuildRoot) { "$mountPoint/$relativeBuildRoot" } else { $mountPoint }
if (-not $OutputPath) {
    $OutputPath = Join-Path $projectDir.FullName 'build\autobuild-mpu.bat'
} elseif (-not [IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $projectDir.FullName $OutputPath
}
$outputDirectory = Split-Path -Parent $OutputPath
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}
if (Test-Path -LiteralPath $OutputPath) {
    $backupDir = Join-Path $projectDir.FullName '.ai_tmp\bak'
    New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    Copy-Item -LiteralPath $OutputPath -Destination (Join-Path $backupDir "autobuild-mpu_$stamp.bat")
}

$template = @'
rem file        autobuild-mpu.bat
rem brief       Build the __PRODUCT__ MPU project in the Docker container
rem note        Generated by hq-autobuild-mpu

@echo off
setlocal

set "DOCKER_CONTAINER_NAME=__CONTAINER__"
set "DOCKER_IMAGE_NAME=__IMAGE__"
set "DOCKER_LOCAL_DEBUG=1"
set "DOCKER_BUILD_DIR=__BUILD_DIR__"
set "G_PROJECT_NAME=__PROJECT__"
set "G_PRODUCT_NAME=__PRODUCT__"
set "G_BUILD_MODEL_OVERRIDE=__MODEL_OVERRIDE__"

echo ============= BEGIN: autobuild-mpu.bat =============
echo DOCKER_CONTAINER_NAME=%DOCKER_CONTAINER_NAME%
echo DOCKER_IMAGE_NAME=%DOCKER_IMAGE_NAME%
echo G_PROJECT_NAME=%G_PROJECT_NAME%
echo G_PRODUCT_NAME=%G_PRODUCT_NAME%
if defined G_BUILD_MODEL_OVERRIDE echo G_BUILD_MODEL_OVERRIDE=%G_BUILD_MODEL_OVERRIDE%

where docker >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker CLI was not found.
    exit /b 1
)
docker info >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker engine is not running or is unavailable.
    echo Start Docker Desktop and retry.
    exit /b 1
)
docker inspect "%DOCKER_CONTAINER_NAME%" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker container "%DOCKER_CONTAINER_NAME%" was not found.
    echo Run tools\docker_cmd\ctp_docker_run.bat to create it.
    exit /b 1
)
for /f %%i in ('docker inspect -f "{{.State.Running}}" "%DOCKER_CONTAINER_NAME%" 2^>nul') do set "CONTAINER_RUNNING=%%i"
if /I not "%CONTAINER_RUNNING%"=="true" (
    echo Docker container "%DOCKER_CONTAINER_NAME%" is stopped; starting it...
    docker start "%DOCKER_CONTAINER_NAME%" >nul
    if errorlevel 1 exit /b 1
)

if defined G_BUILD_MODEL_OVERRIDE (
    docker exec -i -w "%DOCKER_BUILD_DIR%" -e "DOCKER_IMAGE_NAME=%DOCKER_IMAGE_NAME%" -e "DOCKER_LOCAL_DEBUG=%DOCKER_LOCAL_DEBUG%" "%DOCKER_CONTAINER_NAME%" bash "./%G_PROJECT_NAME%/build/mpu/build.sh" "%G_PROJECT_NAME%" "%G_PRODUCT_NAME%" "%G_BUILD_MODEL_OVERRIDE%"
) else (
    docker exec -i -w "%DOCKER_BUILD_DIR%" -e "DOCKER_IMAGE_NAME=%DOCKER_IMAGE_NAME%" -e "DOCKER_LOCAL_DEBUG=%DOCKER_LOCAL_DEBUG%" "%DOCKER_CONTAINER_NAME%" bash "./%G_PROJECT_NAME%/build/mpu/build.sh" "%G_PROJECT_NAME%" "%G_PRODUCT_NAME%"
)
set "BUILD_RESULT=%ERRORLEVEL%"
if not "%BUILD_RESULT%"=="0" (
    echo ERROR: MPU build failed with error code %BUILD_RESULT%.
    exit /b %BUILD_RESULT%
)
echo ============= END: autobuild-mpu.bat =============
exit /b 0
'@

$content = $template.Replace('__CONTAINER__', $containerName).
    Replace('__IMAGE__', $containerImage).
    Replace('__BUILD_DIR__', $dockerBuildDir).
    Replace('__PROJECT__', $projectName).
    Replace('__PRODUCT__', $productName).
    Replace('__MODEL_OVERRIDE__', $(if ($useExplicitBuildModel) { $BuildModel } else { '' }))
$content = $content -replace "(?<!`r)`n", "`r`n"
[IO.File]::WriteAllText($OutputPath, $content, [Text.UTF8Encoding]::new($false))

[pscustomobject]@{
    status = 'success'
    script = (Resolve-Path -LiteralPath $OutputPath).Path
    container = $containerName
    image = $containerImage
    build_dir = $dockerBuildDir
    project = $projectName
    product = $productName
    detected_model = $BuildModel
    model_argument = $(if ($useExplicitBuildModel) { $BuildModel } else { $null })
} | ConvertTo-Json
