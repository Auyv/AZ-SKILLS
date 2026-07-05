---
name: ctp-mpu-debug-build
description: >
  Generate a focused Windows .bat plus Linux container .sh pair for one CTP MPU app,
  then optionally run the generated .bat to verify local Docker-based compilation.
  Use when the user asks to create or run a local MPU app debug build script for
  YLB04-A6, especially based on code/task-jpz_dev/mpu/az_build_linux.bat.
version: 1.0.0

inputs:
  required:
    - name: app_name
      type: string
      description: MPU app directory name under code/task-jpz_dev/mpu/app/.
      validation: Must be a non-empty app directory containing build/build.sh.
  optional:
    - name: workspace
      type: string
      description: Project workspace root.
      default: "C:\\HQ_Workspace\\YLB04_A6"
    - name: execute
      type: boolean
      description: Whether to run the generated .bat immediately after creation.
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
      description: G_PROJECT_NAME and BUILD_PROJECT_NAME. Defaults to app_name when omitted.
      default: "<app_name>"
    - name: container_name
      type: string
      description: Preferred Docker container name.
      default: "YLB04_A6"
    - name: container_id
      type: string
      description: Docker container id confirmed by the user; required when execute=true.
      default: ""
    - name: container_work_dir
      type: string
      description: MPU working directory inside the Docker container.
      default: "/home/sei/code/task-jpz_dev/mpu"

outputs:
  - name: windows_script
    type: file
    description: Generated Windows build entry script.
    path: "<workspace>\\code\\task-jpz_dev\\mpu\\az_build_linux_<app_name>.bat"
  - name: linux_script
    type: file
    description: Generated Linux container build script.
    path: "<workspace>\\code\\task-jpz_dev\\mpu\\az_build_linux_app_<app_name>.sh"
  - name: build_log
    type: file
    description: Captured build log when execute=true.
    path: "<workspace>\\ai_tmp\\mpu_debug_build\\<app_name>_<timestamp>.log"
  - name: status
    type: string
    description: success or fail with the generated paths and exit code.

errors:
  - code: ERR_APP_NAME_EMPTY
    message: app_name is empty.
    handling: Ask for a concrete app directory name or infer it from the user's path.
  - code: ERR_WORKSPACE_NOT_FOUND
    message: Workspace path does not exist.
    handling: Confirm the repository root and retry.
  - code: ERR_APP_BUILD_SCRIPT_MISSING
    message: app/<app_name>/build/build.sh does not exist.
    handling: List nearby app directories and ask the user to choose the correct one.
  - code: ERR_DOCKER_NOT_FOUND
    message: docker command is unavailable or Docker Desktop is not running.
    handling: Start Docker Desktop, then rerun the generated .bat.
  - code: ERR_CONFIRMATION_REQUIRED
    message: execute=true was requested without explicit confirmation of docker id, build model, product name, and project name.
    handling: Ask the user to confirm the four fields, then rerun with -Confirmed.
  - code: ERR_DOCKER_ID_EMPTY
    message: container_id is empty while execute=true.
    handling: Ask the user to confirm the Docker container id.
  - code: ERR_CONTAINER_NOT_FOUND
    message: Neither container_name nor container_id resolves to a container.
    handling: Run tools/docker_cmd/ctp_docker_run.bat or pass the correct container id.
  - code: ERR_BUILD_FAILED
    message: The generated .bat returned a non-zero exit code.
    handling: Preserve the original log and summarize the last meaningful compiler errors.
---

# CTP MPU Debug Build

## What It Does

Create a one-app build entry derived from `code\task-jpz_dev\mpu\az_build_linux.bat`.
The generated Windows script starts or reuses the Docker container, enters
`/home/sei/code/task-jpz_dev/mpu`, and runs a generated Linux shell script that builds only
`app/<app_name>/build/build.sh`.

Use this for fast local verification of an MPU app without editing the shared
`az_build_linux_app.sh` file each time.

## When To Use

Use this skill when the user says things like:

- "给 mcud 生成 MPU 本地编译脚本并跑一下"
- "基于 az_build_linux.bat 做一个 app debug build"
- "快速验证 mpu app 编译"
- "生成 can_server_app 的本地 Docker 编译入口"

Do not use it for full firmware image packaging, FOTA packaging, MCU Keil builds, or Docker image creation.

## How To Do It

1. Read the current project context and locate `code\task-jpz_dev\mpu`.
2. Validate `app/<app_name>/build/build.sh` exists before generating anything.
3. Before every `execute=true` run, ask the user to confirm these four fields in one short message:
   `docker id`, `模组型号(build_model)`, `产品型号(product_name)`, `工程名称(project_name)`.
   If the engineering name equals the app directory, use `app_name` as the project name and do not pass a duplicate `-ProjectName`.
4. Generate scripts with `scripts/generate_and_build.ps1`.
5. If `execute=true`, pass `-Confirmed`, run the generated `.bat`, and capture the log under `ai_tmp\mpu_debug_build`.
6. Report the generated script paths, exit code, and the last relevant log lines on failure.

Preferred command:

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

For script generation only:

```powershell
powershell -ExecutionPolicy Bypass -File <skill_dir>\scripts\generate_and_build.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "mcud" `
  -BuildModel "ag35_cetda" `
  -ProductName "YLB04_A6"
```

## Generated Files

The helper creates two project-local files:

- `code\task-jpz_dev\mpu\az_build_linux_<app_name>.bat`
- `code\task-jpz_dev\mpu\az_build_linux_app_<app_name>.sh`

These files are intentionally app-specific so the existing shared scripts remain unchanged.

## Verification

Treat the build as successful only when:

- the generated `.bat` exits with code `0`
- the log shows the app build script completed
- no Docker/container startup error occurred

If the build fails, preserve the log path and include the original error lines rather than rewriting them.

## File Structure

```text
ctp-mpu-debug-build/
├── SKILL.md
├── MANUAL.md
├── scripts/
│   └── generate_and_build.ps1
├── docs/
│   └── build-script-template.md
└── evals/
    └── evals.json
```
