# Adb Exec Script Template Notes

The generated `.bat` is based on the existing project entry:

```text
code\task-jpz_dev\mpu\adb_push_app.bat
```

Specifically the live (non-commented) mcud push+exec block:

```bat
set APP_NAME=mcud
set TEST_PATH=/media/card
adb shell "rm -rf %TEST_PATH%/%APP_NAME%"
adb push  "%SCRIPT_DIR%/build_output/app/debug/%APP_NAME%"  "%TEST_PATH%"
adb shell "chmod 777 %TEST_PATH%/%APP_NAME%"

adb shell "export LD_LIBRARY_PATH=/vendor/app/hq/lib && %TEST_PATH%/%APP_NAME%"
```

## Key Improvements in the Generated Script

- **App-specific file**: produces `adb_exec_<app_name>.bat` instead of mutating the shared
  `adb_push_app.bat`. Original `adb_push_app.bat` is preserved as the canonical multi-app entry.
- **Parameterized paths**: `TEST_PATH` and `LD_PATH` are explicit so the same template works
  for `/oemdata/`, `/media/card/`, or any other device-side location.
- **LD_LIBRARY_PATH preservation**: appends the configured library path to the existing
  `$LD_LIBRARY_PATH` rather than overwriting it, so platform-shipped libraries stay accessible.
- **CRLF line endings**: the generator normalizes line endings to CRLF via byte operation so
  cmd.exe parses the file reliably on Chinese Windows.
- **Header comment block**: each generated script carries a uniform header (file, brief,
  author, date, version, change log row) so multiple `adb_exec_*.bat` files stay
  distinguishable at a glance.

## Header Block

Each generated `.bat` starts with:

```text
::/**
:: * @file        adb_exec_<app_name>.bat
:: * @brief       adb push and execute <app_name> on device
:: * @note        Derived from adb_push_app.bat (mcud push+exec template).
:: * @author      Mavis (based on adb_push_app.bat)
:: * @date        YYYY/MM/DD
:: * @version     1.0
::
:: * @par         修改日志
:: * <table>
:: * <tr><th>Date         <th>Version     <th>Author      <th> Description
:: * <tr><td>YYYY/MM/DD   <td>1.0         <td>Mavis       <td> 从 adb_push_app.bat 抽出 <app_name> 专用版本
:: * @copyright   xxxx
:: */
```

## Sanity Check Before Push

The script does NOT fail when `build_output/app/debug/<app_name>` is missing. It only emits
a warning. This is intentional because:

- The user may be running this skill before `ctp-mpu-debug-build` (generating scripts first,
  building later).
- The user may have built the artifact via another path (manual docker exec, Jenkins, etc.).

If the build artifact is missing when the user actually runs the `.bat`, the `adb push` step
will fail with a clear "no such file" error.

## Cross-Skill Relationship

- `ctp-mpu-debug-build` produces the build artifact under `build_output/app/debug/<app_name>`.
- `ctp-adb-debug-exec` (this skill) produces the push+exec script that consumes it.

The two skills are independent: the user can use either without the other, but a typical
debug loop uses both in sequence.