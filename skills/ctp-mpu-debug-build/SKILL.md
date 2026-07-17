---
name: ctp-mpu-debug-build
description: >
  Generate a focused Windows .bat plus Linux container .sh pair for one CTP MPU app,
  then optionally run the generated .bat to verify local Docker-based compilation.
  Use when the user asks to create or run a local MPU app debug build script for
  CTP projects using either code/task-mpu or the legacy code/task-jpz_dev/mpu layout.
version: 1.1.0

inputs:
  required:
    - name: app_name
      type: string
      description: MPU app directory name under the detected MPU root's app/ directory.
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
    - name: mpu_relative_path
      type: string
      description: MPU root relative to workspace. Empty means auto-detect code/task-mpu or code/task-jpz_dev/mpu by locating the app build script.
      default: ""
    - name: container_work_dir
      type: string
      description: MPU working directory inside the Docker container. Empty means derive it from mpu_relative_path under /home/sei.
      default: ""

outputs:
  - name: windows_script
    type: file
    description: Generated Windows build entry script.
    path: "<workspace>\\<detected_mpu_relative_path>\\az_build_linux_<app_name>.bat"
  - name: linux_script
    type: file
    description: Generated Linux container build script.
    path: "<workspace>\\<detected_mpu_relative_path>\\az_build_linux_app_<app_name>.sh"
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
  - code: ERR_MPU_DIR_NOT_FOUND
    message: Neither the explicit MPU root nor a supported auto-detected MPU directory exists.
    handling: Confirm workspace or pass the correct relative path with -MpuRelativePath.
  - code: ERR_MPU_PATH_INVALID
    message: MpuRelativePath is absolute instead of workspace-relative.
    handling: Pass a relative path such as code/task-mpu.
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

Create a one-app build entry under the repository's MPU root. Auto-detect
`code\task-mpu` first and retain `code\task-jpz_dev\mpu` as a legacy fallback.
The generated Windows script starts or reuses the Docker container, enters
the matching MPU root under `/home/sei`, and runs a generated Linux shell script that builds only
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

1. Read the current project context and locate `code\task-mpu` or legacy `code\task-jpz_dev\mpu`.
2. Let the helper auto-detect the root from `app/<app_name>/build/build.sh`; pass
   `-MpuRelativePath` only for a non-standard relative layout.
3. Validate `app/<app_name>/build/build.sh` exists before generating anything.
4. Before every `execute=true` run, ask the user to confirm these four fields in one short message:
   `docker id`, `模组型号(build_model)`, `产品型号(product_name)`, `工程名称(project_name)`.
   If the engineering name equals the app directory, use `app_name` as the project name and do not pass a duplicate `-ProjectName`.
5. Generate scripts with `scripts/generate_and_build.ps1`.
6. If `execute=true`, pass `-Confirmed`, run the generated `.bat`, and capture the log under `ai_tmp\mpu_debug_build`.
7. Report the detected MPU root, generated script paths, exit code, and the last relevant log lines on failure.

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

- `<detected_mpu_relative_path>\az_build_linux_<app_name>.bat`
- `<detected_mpu_relative_path>\az_build_linux_app_<app_name>.sh`

The files are created in the auto-detected MPU root (`code\task-mpu` or the legacy
`code\task-jpz_dev\mpu`). They are intentionally app-specific so existing shared scripts remain unchanged.

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
