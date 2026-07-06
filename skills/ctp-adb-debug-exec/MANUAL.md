# ctp-adb-debug-exec Manual

## Purpose

Generate a focused Windows `.bat` that adb-pushes one MPU app build artifact to a
connected device and immediately executes it. Derived from the existing
`code\task-jpz_dev\mpu\adb_push_app.bat` so the shared entry script stays untouched.

## Manual Usage

From the repository root:

```powershell
# Generate only (recommended; user runs the .bat manually when device is ready)
powershell -ExecutionPolicy Bypass -File .\skills\ctp-adb-debug-exec\scripts\generate_adb_exec.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "spc_mcud_clt_test"
```

Generate with custom device path / library path:

```powershell
powershell -ExecutionPolicy Bypass -File .\skills\ctp-adb-debug-exec\scripts\generate_adb_exec.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "mcud" `
  -DevicePath "/oemdata" `
  -LdLibraryPath "/vendor/app/hq/lib"
```

Generate and immediately run (device must already be connected via adb):

```powershell
powershell -ExecutionPolicy Bypass -File .\skills\ctp-adb-debug-exec\scripts\generate_adb_exec.ps1 `
  -Workspace "C:\HQ_Workspace\YLB04_A6" `
  -AppName "mcud" `
  -Execute
```

## Inputs

- `Workspace`: repository root, default `C:\HQ_Workspace\YLB04_A6`
- `AppName`: app name whose `build_output/app/debug/<AppName>` artifact is pushed
- `DevicePath`: target directory on device, default `/media/card`
- `LdLibraryPath`: library path prepended to `LD_LIBRARY_PATH` on device, default `/vendor/app/hq/lib`
- `Execute`: run the generated `.bat` immediately (off by default; adb push is blocking)

## Outputs

- `code\task-jpz_dev\mpu\adb_exec_<AppName>.bat` — generated `.bat` (always CRLF)

## Failure Handling

If `AppName` is empty, the script exits with `ERR_APP_NAME_EMPTY`.

If the workspace path does not exist, the script exits with `ERR_WORKSPACE_NOT_FOUND`.

If the build artifact under `build_output/app/debug/<AppName>` is missing, the script prints
a warning but still generates the `.bat`. The user can rebuild via the `ctp-mpu-debug-build`
skill before pushing.

If `adb` is unavailable or no device is connected, the generated `.bat` will fail at the
`adb push` step. The error message from `adb` is preserved verbatim.

## Validation Checklist

After running the generator, verify:

1. The file `<workspace>\code\task-jpz_dev\mpu\adb_exec_<AppName>.bat` exists.
2. The file uses CRLF line endings (the script enforces this; manual edits should keep it).
3. The first 4 bytes of the file are `0D 0A 3A 3A` (CRLF then `::`).
4. Running `adb devices` before executing shows the target device in `device` state.

## Cross-Skill Workflow

`ctp-adb-debug-exec` is intentionally independent from `ctp-mpu-debug-build`. A typical
debug loop is:

1. `ctp-mpu-debug-build` to compile the app → produces `build_output/app/debug/<AppName>`
2. `ctp-adb-debug-exec` to generate the push+exec `.bat`
3. Connect device, run the generated `.bat`, observe logs on stdout