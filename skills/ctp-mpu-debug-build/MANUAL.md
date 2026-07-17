# ctp-mpu-debug-build Manual

## Purpose

Generate a focused local build script for one MPU app and optionally run it through Docker.
The helper auto-detects `code\task-mpu` and the legacy `code\task-jpz_dev\mpu` layout.

## Manual Usage

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\skills\ctp-mpu-debug-build\scripts\generate_and_build.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "mcud" `
  -ContainerId "6e10b5571b75d05d434742663a15b0188157d431b8e13c6d93a09585e4ac9986" `
  -BuildModel "ag35_cetda" `
  -ProductName "YLB04_A6" `
  -Confirmed `
  -Execute
```

Generate only:

```powershell
powershell -ExecutionPolicy Bypass -File .\skills\ctp-mpu-debug-build\scripts\generate_and_build.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "mcud" `
  -BuildModel "ag35_cetda" `
  -ProductName "YLB04_A6"
```

## Inputs

Before every execution, confirm these four values with the user:

- `docker id`
- `模组型号` / `BuildModel`
- `产品型号` / `ProductName`
- `工程名称` / `ProjectName`，默认等于 `AppName`，不一致时才传 `-ProjectName`

- `Workspace`: repository root, default `C:\HQ_Workspace\YLB04_A6`
- `AppName`: app directory under the detected MPU root's `app` directory
- `Execute`: run generated `.bat` after writing scripts
- `ContainerName`: default `YLB04_A6`
- `ContainerId`: required when `Execute` is used
- `MpuRelativePath`: optional workspace-relative MPU root; empty auto-detects `code\task-mpu` or legacy `code\task-jpz_dev\mpu`
- `ContainerWorkDir`: optional container MPU root; empty derives `/home/sei/<MpuRelativePath>`
- `ProductName`: default `YLB04_A6`
- `BuildModel`: default `ag35_cetda`
- `ProjectName`: project name exported as `G_PROJECT_NAME`; defaults to `AppName`
- `Confirmed`: required when `Execute` is used

## Outputs

- `<detected MPU root>\az_build_linux_<AppName>.bat`
- `<detected MPU root>\az_build_linux_app_<AppName>.sh`
- `ai_tmp\mpu_debug_build\<AppName>_<timestamp>.log` when `-Execute` is used

## Failure Handling

If generation fails, check whether a supported MPU root exists and whether
`app\<AppName>\build\build.sh` exists below it. For a non-standard layout, pass for example
`-MpuRelativePath "src\mpu"`; absolute paths are rejected.

If execution fails, open the log printed by the script and inspect the original Docker or compiler
error. Do not discard or beautify compiler output.
