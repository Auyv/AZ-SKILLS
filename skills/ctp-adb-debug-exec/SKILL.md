---
name: ctp-adb-debug-exec
description: >
  Generate a focused Windows .bat that adb-pushes one CTP MPU app build artifact to a
  connected device and immediately executes it. Use when the user asks for a single-app
  adb push+exec script derived from code/task-jpz_dev/mpu/adb_push_app.bat, especially
  for spc_mcud_clt_test / mcud debug runs.
version: 1.0.0

inputs:
  required:
    - name: app_name
      type: string
      description: MPU app name whose build_output/app/debug/<app_name> artifact will be pushed.
      validation: Build artifact at <workspace>/code/task-jpz_dev/mpu/build_output/app/debug/<app_name> is recommended; a missing artifact only emits a warning.
  optional:
    - name: workspace
      type: string
      description: Project workspace root.
      default: "C:\\HQ_Workspace\\YLB04_A6"
    - name: device_path
      type: string
      description: Target directory on the device for push + exec.
      default: "/media/card"
    - name: ld_library_path
      type: string
      description: Library path prepended to LD_LIBRARY_PATH on the device.
      default: "/vendor/app/hq/lib"
    - name: execute
      type: boolean
      description: Whether to run the generated .bat immediately. Off by default because adb pushes block on a real device.
      default: false

outputs:
  - name: windows_script
    type: file
    description: Generated Windows adb push+exec script.
    path: "<workspace>\\code\\task-jpz_dev\\mpu\\adb_exec_<app_name>.bat"

errors:
  - code: ERR_APP_NAME_EMPTY
    message: app_name is empty.
    handling: Ask for a concrete app directory name or infer it from the user's path.
  - code: ERR_WORKSPACE_NOT_FOUND
    message: Workspace path does not exist.
    handling: Confirm the repository root and retry.
  - code: ERR_ADB_NOT_FOUND
    message: adb command is unavailable or no device is connected.
    handling: Start adb server, connect device, then rerun the generated .bat.
---

# CTP ADB Debug Exec

## What It Does

Create a one-app adb push + execute entry derived from `code\task-jpz_dev\mpu\adb_push_app.bat`.
The generated Windows script:

1. Removes any existing copy at `<device_path>/<app_name>` on the device
2. Pushes `build_output/app/debug/<app_name>` to `<device_path>/<app_name>`
3. chmod 777 on the pushed binary
4. Exports `LD_LIBRARY_PATH=<ld_library_path>:$LD_LIBRARY_PATH` and runs the binary in foreground

Use this for fast local device smoke tests of one MPU app without editing the shared
`adb_push_app.bat` each time.

## When To Use

Use this skill when the user says things like:

- "做一个 adb_exec_spc_mcud_clt_test.bat"
- "生成 mcud 的 adb push + 执行脚本"
- "把 spc_mcud_clt_test 推上设备跑一下"

Do not use it for:

- mass-deploying to many devices (production OTA / Jenkins pipeline)
- pulling logs from device (use dedicated logging skill)
- multi-app orchestration in a single run

## How To Do It

1. Read the current project context and locate `code\task-jpz_dev\mpu`.
2. Sanity-check `build_output/app/debug/<app_name>` exists. Warn (do not fail) if missing.
3. Generate the `.bat` with `scripts/generate_adb_exec.ps1`. The output is always CRLF-terminated.
4. If `-Execute` is set, run the generated `.bat` directly. Off by default because adb push is blocking on a real device.

Preferred command (generate only):

```powershell
powershell -ExecutionPolicy Bypass -File <skill_dir>\scripts\generate_adb_exec.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "spc_mcud_clt_test"
```

Generate with custom device path / library path:

```powershell
powershell -ExecutionPolicy Bypass -File <skill_dir>\scripts\generate_adb_exec.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "mcud" `
  -DevicePath "/oemdata" `
  -LdLibraryPath "/vendor/app/hq/lib"
```

Generate with custom mpu subdirectory (avoid hardcoded 'code\task-jpz_dev\mpu'):

```powershell
powershell -ExecutionPolicy Bypass -File <skill_dir>\scripts\generate_adb_exec.ps1 `
  -Workspace "C:\HQ_Workspace\hqt401_k00" `
  -AppName "atserver_app-master" `
  -MpuDir "code\task-dev\mpu"
```

Use -MpuDir to override the default subdirectory 'code\task-jpz_dev\mpu'. Useful when the project uses a different layout (e.g. 'code\task-dev\mpu').

Generate and immediately run (device must already be connected via adb):

```powershell
powershell -ExecutionPolicy Bypass -File <skill_dir>\scripts\generate_adb_exec.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "mcud" `
  -Execute
```

## Generated File

The helper creates exactly one project-local file:

- `code\task-jpz_dev\mpu\adb_exec_<app_name>.bat`

The file is intentionally app-specific so the shared `adb_push_app.bat` remains unchanged.

## Generated Script Structure

The generated `.bat` is always CRLF-terminated and contains:

- File header comment block (file name, brief, author, date, version, change log table)
- Argument inspection loop (kept for parity with `adb_push_app.bat`)
- `SCRIPT_DIR` / `CURRENT_DIR` echo
- adb sequence: `rm -rf` → `push` → `chmod 777` → `export LD_LIBRARY_PATH && exec`
- Trailer with errorlevel echo and `pause`

## Verification

Treat the generated `.bat` as ready when:

- the file exists at the expected path
- its line count matches `bat_lines = body_lines` after CRLF normalization (no LF-only lines)
- `head -c 4` of the file shows `0D 0A 3A 3A` (CRLF followed by `::` comment)

If generation fails, surface the original PowerShell error rather than wrapping it.

## File Structure

```text
ctp-adb-debug-exec/
├── SKILL.md
├── MANUAL.md
├── scripts/
│   └── generate_adb_exec.ps1
├── docs/
│   └── adb-exec-template.md
└── evals/
    └── evals.json
```

## Notes

- This skill is independent from `ctp-mpu-debug-build`. The user is expected to have already built the app via that skill (or another path) so the artifact exists under `build_output/app/debug/`.
- The generated `.bat` always preserves any pre-existing `LD_LIBRARY_PATH` on the device by appending to it via `$LD_LIBRARY_PATH`, instead of overwriting.
- Output uses CRLF regardless of host platform so cmd.exe parses it on Chinese Windows correctly.