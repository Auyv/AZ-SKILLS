# ctp-mpu-debug-build Manual

## Purpose

Generate and optionally run one self-contained Windows BAT for a single MPU app. It uses a fixed Docker container ID, runs the app build inside the container, and saves a build log. No companion `.sh` or Docker configuration directory is created.

## Usage

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

Remove `-Execute` to generate the BAT without running Docker.

Before execution, confirm the container ID, model, product, and project. Project name defaults to AppName.

## Inputs

- `Workspace`: repository root.
- `AppName`: app directory under the detected MPU root.
- `ContainerId`: fixed Docker container ID.
- `MpuRelativePath`: optional workspace-relative MPU root; otherwise auto-detected.
- `ContainerWorkDir`: optional container MPU root; otherwise derived under `/home/sei`.
- `StagingDir`: container sysroot; defaults to `/home/sdk/ql-sysroots`.
- `CanServerPerfTestEnable`: `ON` or `OFF`, defaults to `OFF`.
- `ProductName`, `BuildModel`, `ProjectName`: values exported into the app build.
- `Confirmed`: required with `Execute` after user confirmation.

## Outputs

- `<detected MPU root>\az_build_linux_<AppName>.bat`
- `ai_tmp\mpu_debug_build\<AppName>_<timestamp>.log`

The BAT checks Docker, starts a stopped container, waits for readiness, invokes `app/<AppName>/build/build.sh`, and returns its exit code.

## Verification record

On 2026-10-08, the generated single BAT was run for `can_server_app` with product `HQT420-S11-02`, model `ag35_ceraa`, and the CAN performance test plugin enabled. The BAT returned exit code 0, the build log reported `[100%] Built target can_server_app`, and the output was confirmed as an ARM ELF32 executable. The compiler emitted product-macro whitespace warnings from `-D HQT420-S11-02`; there were no compiler errors.
