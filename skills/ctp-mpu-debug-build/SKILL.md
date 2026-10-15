---
name: ctp-mpu-debug-build
description: >
  Generate one self-contained Windows BAT for a CTP MPU app and optionally run it
  to verify a Docker-based debug build.
version: 1.2.1

inputs:
  required:
    - name: app_name
      type: string
      description: MPU app directory under the detected MPU root's app directory.
      validation: Must contain build/build.sh.
  optional:
    - name: workspace
      type: string
      description: Project workspace root.
      default: "C:\\HQ_Workspace\\YLB04_A6"
    - name: execute
      type: boolean
      description: Run the generated BAT immediately after creation.
      default: true
    - name: product_name
      type: string
      description: G_PRODUCT_NAME and BUILD_PRODUCT_NAME.
      default: "YLB04_A6"
    - name: build_model
      type: string
      description: G_BUILD_MODEL and BUILD_PLATFORM_NAME.
      default: "ag35_cetda"
    - name: project_name
      type: string
      description: G_PROJECT_NAME and BUILD_PROJECT_NAME; defaults to app_name.
      default: "<app_name>"
    - name: container_id
      type: string
      description: Fixed Docker container ID; required when execute=true.
      default: ""
    - name: mpu_relative_path
      type: string
      description: MPU root relative to workspace; empty auto-detects supported layouts.
      default: ""
    - name: container_work_dir
      type: string
      description: Container MPU root; empty derives it under /home/sei.
      default: ""
    - name: staging_dir
      type: string
      description: Sysroot path inside the container.
      default: "/home/sdk/ql-sysroots"
    - name: can_server_perf_test_enable
      type: string
      description: Enable the can_server_app performance test plugin.
      default: "OFF"

outputs:
  - name: windows_script
    type: file
    description: Self-contained Windows build entry script.
    path: "<workspace>\\<detected_mpu_relative_path>\\az_build_linux_<app_name>.bat"
  - name: build_log
    type: file
    description: Build log created by the BAT.
    path: "<workspace>\\ai_tmp\\mpu_debug_build\\<app_name>_<timestamp>.log"
  - name: status
    type: string
    description: Build result, output path, and exit code.

errors:
  - code: ERR_APP_NAME_EMPTY
    message: app_name is empty.
    handling: Provide a concrete app directory name.
  - code: ERR_WORKSPACE_NOT_FOUND
    message: Workspace does not exist.
    handling: Confirm the repository root.
  - code: ERR_APP_BUILD_SCRIPT_MISSING
    message: app/<app_name>/build/build.sh does not exist.
    handling: Select the correct app directory.
  - code: ERR_MPU_DIR_NOT_FOUND
    message: No supported MPU root was found.
    handling: Set MpuRelativePath to the workspace-relative MPU root.
  - code: ERR_MPU_PATH_INVALID
    message: MpuRelativePath is absolute.
    handling: Use a path relative to Workspace.
  - code: ERR_DOCKER_NOT_FOUND
    message: Docker is unavailable.
    handling: Start Docker Desktop and rerun the BAT.
  - code: ERR_CONFIRMATION_REQUIRED
    message: execute=true requires confirmation of container ID, build model, product, and project.
    handling: Confirm the values, then run with -Confirmed.
  - code: ERR_DOCKER_ID_EMPTY
    message: ContainerId is required when Execute is used.
    handling: Supply the fixed Docker container ID.
  - code: ERR_CONTAINER_NOT_FOUND
    message: The fixed container ID does not identify a Docker container.
    handling: Confirm the ID and Docker availability.
  - code: ERR_BUILD_FAILED
    message: The BAT returned a non-zero exit code.
    handling: Preserve its log and report the original compiler or Docker errors.
---

# CTP MPU Debug Build

## What it does

Creates one app-specific Windows BAT. The BAT addresses the confirmed Docker container ID directly, starts it if needed, passes build settings into the container, invokes `app/<app_name>/build/build.sh`, and saves a build log under `ai_tmp/mpu_debug_build`. It does not create a companion shell file or Docker CLI config directory.

Use it for local MPU app builds, not firmware image packaging, FOTA packaging, MCU builds, or Docker image creation.

## Procedure

1. Find `code/task-mpu` or the legacy `code/task-jpz_dev/mpu`, and confirm `app/<app_name>/build/build.sh` exists.
2. Before every execute run, confirm the Docker container ID, build model, product name, and project name. Project name defaults to app_name.
3. Generate the BAT with `scripts/generate_and_build.ps1`.
4. For execution, pass `-Confirmed`; the BAT validates and starts the fixed container, runs the app build, saves and prints the log, and returns the build exit code.
5. Report the detected MPU root, BAT path, log path, and exit code.

Example:

```powershell
powershell -ExecutionPolicy Bypass -File <skill_dir>\scripts\generate_and_build.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "mcud" `
  -ContainerId "6e10b5571b75d05d434742663a15b0188157d431b8e13c6d93a09585e4ac9986" `
  -BuildModel "ag35_cetda" `
  -ProductName "YLB04_A6" `
  -Confirmed `
  -Execute
```

For `can_server_app`, use `-CanServerPerfTestEnable ON` to compile the performance test plugin.

The helper creates only `az_build_linux_<app_name>.bat` under the detected MPU root. The BAT embeds environment setup and Docker execution, so no companion file is required.

Treat a build as successful only when the BAT exits with code zero, its log shows the app build completed, and Docker startup succeeded. Preserve the original log on failure.
