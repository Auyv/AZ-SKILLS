---
name: az-keil-build
description: |
  Use this skill when the user asks to build, fix, or verify a Keil MDK/uVision
  embedded firmware project from the command line, especially when the success
  criterion is "0 warning 0 error". Prefer compiling a specified .uvprojx file
  directly with UV4.exe, auto-detect the output artifacts, and keep batch files
  such as build_keil.bat as a generated project-local bootstrap/fallback entry.
version: 1.3.0
inputs:
  required:
    - name: workspace_path
      type: string
      description: Absolute path to the firmware workspace.
      validation: Must exist and contain a Keil project or an explicit build command.
    - name: project_file
      type: string
      description: Keil project file path, normally project/mdk/*.uvprojx.
      validation: Must be a readable .uvprojx file. If omitted by the user, discover it only when exactly one candidate exists.
  optional:
    - name: target_name
      type: string
      description: Keil target name. Defaults to the first Target in the project.
      default: null
    - name: uv4_path
      type: string
      description: Absolute path to UV4.exe. Defaults to PATH and common Keil install paths.
      default: null
    - name: build_command
      type: string
      description: Compatibility fallback command, such as .\build_keil.bat, used only when project_file is unavailable.
      default: null
    - name: build_script_path
      type: string
      description: Path for the generated project-local build script.
      default: build_keil.bat
    - name: init_build_script
      type: boolean
      description: Create a project-local build_keil.bat template without compiling.
      default: false
    - name: log_file
      type: string
      description: Build log path to inspect after compilation.
      default: project directory __build/keil_build.log for direct UV4 builds; build_keil.log for fallback command builds
    - name: rebuild
      type: boolean
      description: Force UV4 rebuild mode. Direct project builds default to rebuild so post-build flashing artifacts are refreshed.
      default: true
    - name: build_only
      type: boolean
      description: Use UV4 build mode instead of rebuild mode when a quick incremental compile is explicitly desired.
      default: false
    - name: resolve_only
      type: boolean
      description: Parse target output information without invoking Keil.
      default: false
    - name: target_result
      type: string
      description: Expected final build result.
      default: 0 warning 0 error
outputs:
  - name: status
    type: string
    description: success or error.
  - name: timestamp
    type: string
    description: ISO-like local timestamp when the result was produced.
  - name: build_result
    type: object
    description: Parsed warning/error count, summary line, log path, command, and selected target.
  - name: artifact_info
    type: object
    description: Auto-detected output directory and artifact paths, including axf_file, hex_file, bin_files, and artifact_files.
  - name: build_script
    type: object
    description: Generated or existing build script path and setup message for first-run bootstrap.
  - name: changed_files
    type: list
    description: Files modified to fix compilation.
  - name: verification_summary
    type: string
    description: Final command and exact log evidence proving the result.
errors:
  - code: ERR_PROJECT_FILE_MISSING
    message: The requested .uvprojx file does not exist.
    handling: Search for nearby .uvprojx files or ask the user to pass an explicit project_file.
  - code: ERR_PROJECT_AMBIGUOUS
    message: More than one .uvprojx file exists and no project_file was specified.
    handling: Stop and request the intended project_file instead of guessing.
  - code: ERR_TARGET_NOT_FOUND
    message: The requested Keil target is not present in the project.
    handling: List available TargetName values and ask for or choose the correct target.
  - code: ERR_BUILD_SCRIPT_MISSING
    message: The configured build command or build_keil.bat does not exist.
    handling: Use direct .uvprojx compilation when possible; otherwise locate the correct fallback command before editing source.
  - code: ERR_KEIL_NOT_AVAILABLE
    message: UV4.exe or required Keil toolchain cannot be launched.
    handling: Report the raw command output and stop unless an alternate local build path exists.
  - code: ERR_UV4_PATH_REQUIRED
    message: UV4.exe was not found during first-run setup.
    handling: Create build_keil.bat when missing and tell the user to fill UV4_EXE_FULL_PATH, pass -Uv4Path, or set KEIL_UV4_PATH.
  - code: ERR_STALE_LOG
    message: The build command exits but the log timestamp or content did not change.
    handling: Verify project encoding, command invocation, and UV4 failure mode before trusting the log.
  - code: ERR_COMPILE_ERRORS_REMAIN
    message: Compiler or linker errors remain after a build.
    handling: Fix the first real root-cause group, rebuild, and repeat.
  - code: ERR_WARNINGS_REMAIN
    message: Build succeeds but warnings remain.
    handling: Treat warnings as defects and continue until warning count is zero.
  - code: ERR_PERMISSION_DENIED
    message: The workspace, backup directory, or build output path cannot be written.
    handling: Report the blocked path and ask for permission or a writable workspace.
  - code: ERR_ENCODING_RISK
    message: A source or project file may be damaged by the wrong text encoding.
    handling: Identify encoding before editing and prefer focused patches or no-BOM UTF-8 for Keil XML.
  - code: ERR_SDK_MODIFICATION_RISK
    message: The apparent fix requires editing SDK/vendor/prebuilt delivery files.
    handling: Stop and use project-owned adapters, wrappers, or configuration unless the user explicitly approves SDK edits.
  - code: ERR_OUTPUT_ARTIFACT_MISSING
    message: The build is clean but the expected axf/hex/bin artifact cannot be found.
    handling: Re-parse TargetCommonOption, inspect post-build steps, and report artifact discovery gaps before J-Link flashing.
---

# Keil Build Fix Workflow

## What This Skill Does

This skill drives a Keil MDK firmware build to a clean result from the command
line. It is intended for embedded C projects where a `.uvprojx` can be compiled
directly with `UV4.exe` and the user expects the final build log to show exactly
`0 Error(s), 0 Warning(s)`.

It also resolves build artifact locations from the Keil target settings so a
later J-Link flashing step can consume `axf_file`, `hex_file`, or `bin_files`
without hard-coded project paths.

On first use for a new project, create a project-local `build_keil.bat` template
when it does not already exist. The template should contain the selected
`.uvprojx`, target, log path, and a `PLEASE_FILL_UV4_EXE_FULL_PATH` placeholder
that the user fills once for that machine.

## When To Use

Use this skill when the user says things like:

- "帮我解决 Keil 编译报错"
- "用 build_keil.bat 编译到 0 warning 0 error"
- "编译指定的 application.uvprojx，并告诉我 axf 在哪里"
- "修 ARMCC/Keil/uVision 编译错误"
- "今天改了组件，帮我把工程编过"
- "先解析 Keil 编译产物位置，后面我要用 J-Link 烧写"
- "第一次接入这个 Keil 工程，先帮我生成 build_keil.bat"

Do not use it for generic CMake, Makefile, GCC-only, or cloud CI failures unless
the repo also has a Keil/uVision build flow.

## Required Discipline

1. Read local project instructions first, especially `AGENTS.md` and any global
   rules they reference.
2. Preserve SDK/vendor/prebuilt directories unless the project explicitly allows
   edits there.
3. Check `git status --short` before editing. Treat pre-existing changes as
   user work and do not revert them.
4. Before modifying an existing file, make a timestamped or path-preserving
   backup under `ai_tmp/bak/` when the project rules require it.
5. Respect source encoding. Keil projects often mix GBK/GB2312 source with UTF-8
   XML; avoid rewriting whole files when a focused patch is enough.
6. Keep the user-facing success criterion strict: zero errors and zero warnings
   in the newest log, not just a passing process exit code.

## Recommended Tooling

Prefer direct project compilation:

```powershell
.\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 `
  -WorkspacePath . `
  -ProjectFile .\project\mdk\application.uvprojx `
  -TargetName application
```

First-run bootstrap for a new project:

```powershell
.\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 `
  -WorkspacePath . `
  -ProjectFile .\project\mdk\application.uvprojx `
  -InitBuildScript
```

After this, tell the user to edit generated `build_keil.bat` and fill:

```bat
set "UV4_EXE_FULL_PATH=PLEASE_FILL_UV4_EXE_FULL_PATH"
```

Resolve artifacts without compiling:

```powershell
.\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 `
  -WorkspacePath . `
  -ProjectFile .\project\mdk\application.uvprojx `
  -ResolveOnly
```

Use a legacy batch only as a fallback:

```powershell
.\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 `
  -WorkspacePath . `
  -BuildCommand .\build_keil.bat `
  -LogFile .\build_keil.log
```

The helper emits JSON containing `status`, `timestamp`, `errors`, `warnings`,
`summary`, `log_file`, `build_command`, and `project` artifact fields such as
`output_directory`, `axf_file`, `hex_file`, `post_build_outputs`, `bin_files`,
and `artifact_files`. During first-run setup it also emits `build_script.path`
and whether the script was created or already existed.
It is a convenience wrapper only; the agent still needs to inspect the log and
source when the build is not clean.

## Build Loop

1. Identify the intended `.uvprojx` file. If the user did not specify one,
   auto-select only when exactly one candidate exists.
2. Parse the selected target from the project XML:
   `TargetName`, `OutputDirectory`, `OutputName`, `CreateExecutable`, and
   `CreateHexFile`.
3. If this is the first run for the project and no local build script exists,
   create `build_keil.bat` from the selected project settings. Do not overwrite
   an existing script.
4. Tell the user to fill `UV4_EXE_FULL_PATH` in `build_keil.bat` when UV4 cannot
   be auto-detected, or to pass `-Uv4Path` / set `KEIL_UV4_PATH`.
5. Run direct UV4 compilation from the workspace root. Use rebuild by default
   because Keil post-build steps often generate the BIN used by flashing:

   ```powershell
   UV4.exe -j0 -r path\to\project.uvprojx -t TargetName -o path\to\keil_build.log
   ```

6. If no project file is available, run the compatibility build command such as
   `.\build_keil.bat`.
7. Record the build log timestamp before and after the command when practical.
8. Read the build log and capture the exact summary line. Do not trust only the
   process exit code because batch wrappers may return success even when Keil
   fails.
9. If the log did not change, compare timestamps and inspect whether `UV4.exe`
   failed before writing a fresh log. A stale log is a separate failure mode.
10. Group errors by root cause instead of fixing lines mechanically:
   missing files/groups, include path mismatch, type/API mismatch, syntax
   errors, then linker unresolved symbols.
11. Fix the smallest coherent root-cause group and rebuild.
12. After errors are gone, continue resolving every warning.
13. Stop only when the latest log contains a clean Keil summary and artifact
    discovery has been refreshed:

   ```text
   0 Error(s), 0 Warning(s).
   ```

## Common Keil Failure Patterns

- Missing source listed in `.uvprojx`: remove the stale group or add the intended
  replacement file, matching the project crop.
- Include path mismatch: prefer fixing the Keil include path or adding a narrow
  compatibility wrapper over scattering relative includes through source files.
- Generated component headers missing: add minimal compatibility headers only in
  project-owned framework/business directories, not SDK.
- ARMCC warning cleanup: add final newlines, cast intentionally ignored returns,
  remove unused locals, and align function declarations with headers.
- Unresolved symbols from excluded components: first verify whether the real
  component is intentionally excluded. If it is, use project-owned weak stubs
  only when that matches local conventions and the missing behavior is safely
  disabled.
- Project XML corruption or silent UV4 failure: check for BOM/encoding changes
  and restore a narrow diff before continuing.
- GBK/GB2312 source warnings after UTF-8 edits: preserve the local encoding or
  keep new strings ASCII when the file is not UTF-8.
- Clean build but no flashable artifact: inspect whether the target emits AXF
  only, HEX generation is disabled, or BIN is produced by a post-build step.

## Artifact Discovery

For direct `.uvprojx` builds, parse artifact paths from the selected target:

- `output_directory`: `TargetOption/TargetCommonOption/OutputDirectory`,
  resolved relative to the `.uvprojx` directory.
- `output_name`: `TargetOption/TargetCommonOption/OutputName`.
- `axf_file`: `output_directory + output_name + ".axf"` when
  `CreateExecutable` is enabled.
- `hex_file`: `output_directory + output_name + ".hex"` when `CreateHexFile`
  is enabled.
- `bin_files`: existing `*.bin` files found in the output directory.
- `post_build_outputs`: paths parsed from `AfterMake/UserProg*Name` commands
  such as `fromelf --output=...`.
- `artifact_files`: existing `*.axf`, `*.hex`, `*.bin`, and `*.elf` files found
  in the output directory or post-build output list.

For J-Link flashing, prefer `hex_file` when the project explicitly emits HEX.
Otherwise use `axf_file` if the flashing flow accepts ELF/AXF symbols, or find
the post-build generated BIN if the project uses one.

## UV4 Path Policy

Resolve `UV4.exe` in this order:

1. Explicit `-Uv4Path`.
2. `KEIL_UV4_PATH` environment variable.
3. `UV4.exe` in `PATH`.
4. Common Keil install paths.
5. Generated `build_keil.bat` with `UV4_EXE_FULL_PATH` placeholder for the user
   to fill.

This keeps the shared skill portable across different developer machines while
still leaving each project with a repeatable local build entry.

## Edit Strategy

Prefer fixes in this order:

1. Project configuration and include paths when the source is already correct.
2. Project-owned adapter or compatibility headers when generated components have
   moved paths.
3. Local source fixes that match the declared headers and existing APIs.
4. Weak stubs only for intentionally excluded optional components.
5. SDK/vendor changes only after explicit user approval.

## Output Format

When finishing, report:

- Build command used.
- Final warning/error summary copied from the latest log.
- Main files changed.
- Any residual risk, such as weak stubs, intentionally excluded components, or
  generated build artifacts left untouched.

Use this logical response shape:

```json
{
  "status": "success",
  "timestamp": "YYYY-MM-DD HH:mm:ss",
  "data": {
    "build_command": ".\\build_keil.bat",
    "log_file": "build_keil.log",
    "errors": 0,
    "warnings": 0,
    "summary": "0 Error(s), 0 Warning(s).",
    "artifact_info": {
      "output_directory": "project\\mdk\\Objects",
      "axf_file": "project\\mdk\\Objects\\application.axf",
      "hex_file": null,
      "post_build_outputs": ["project\\mdk\\__build\\bin\\application.bin"],
      "bin_files": ["project\\mdk\\__build\\bin\\application.bin"]
    },
    "changed_files": []
  }
}
```

On failure, set `status` to `error` and include `error.code`, `error.message`,
and the raw evidence line or command output that led to the decision.
