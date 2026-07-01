# az-keil-build Manual

## Goal

Drive a specified Keil MDK `.uvprojx` project to a clean command-line build with:

```text
0 Error(s), 0 Warning(s).
```

Also resolve output artifacts such as `.axf`, `.hex`, and `.bin` for later
J-Link flashing.

## Manual Steps

1. Open PowerShell at the workspace root.
2. Read project instructions:

   ```powershell
   Get-Content -Raw .\AGENTS.md
   ```

3. Check current worktree state:

   ```powershell
   git status --short
   ```

4. Build a specific Keil project directly:

   ```powershell
   .\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 -WorkspacePath . -ProjectFile .\project\mdk\application.uvprojx
   ```

5. On first use for a project, create a local `build_keil.bat` template:

   ```powershell
   .\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 -WorkspacePath . -ProjectFile .\project\mdk\application.uvprojx -InitBuildScript
   ```

6. Open the generated `build_keil.bat` and fill the machine-local Keil path:

   ```bat
   set "UV4_EXE_FULL_PATH=C:\Your\Keil\UV4\UV4.exe"
   ```

7. Pick a specific target when the project has multiple targets:

   ```powershell
   .\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 -WorkspacePath . -ProjectFile .\project\mdk\application.uvprojx -TargetName application
   ```

8. Use incremental build only when post-build artifacts do not need refresh:

   ```powershell
   .\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 -WorkspacePath . -ProjectFile .\project\mdk\application.uvprojx -BuildOnly
   ```

9. Resolve output paths without compiling:

   ```powershell
   .\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 -WorkspacePath . -ProjectFile .\project\mdk\application.uvprojx -ResolveOnly
   ```

10. Use a legacy batch only when direct project compilation is not available:

   ```powershell
   .\skills\az-keil-build\scripts\Invoke-KeilBuild.ps1 -WorkspacePath . -BuildCommand .\build_keil.bat -LogFile .\build_keil.log
   ```

11. Inspect the final summary:

   ```powershell
   Select-String -Path .\project\mdk\__build\keil_build.log -Pattern 'Error\(s\),.*Warning\(s\)'
   ```

12. If errors exist, fix the first root-cause group and rebuild.
13. If warnings exist, fix them as build blockers and rebuild.
14. Before editing existing files, create backups under `ai_tmp/bak/` if required
   by project rules.
15. Do not edit SDK/prebuilt delivery directories unless explicitly approved.
16. Stop only after the latest log shows `0 Error(s), 0 Warning(s)` and the
   structured output reports the artifact paths needed by the flash step.

## Artifact Fields

- `project.output_directory`: absolute output directory resolved from
  `OutputDirectory`.
- `project.axf_file`: expected AXF path from `OutputName`.
- `project.hex_file`: expected HEX path when `CreateHexFile` is enabled.
- `project.post_build_outputs`: output paths parsed from Keil post-build
  commands such as `fromelf --output=...`.
- `project.bin_files`: existing BIN files found in the output directory.
- `project.artifact_files`: existing AXF/HEX/BIN/ELF files found in the output
  directory and parsed post-build outputs.

## Troubleshooting Notes

- If `build_keil.bat` exits with code 0 but the log still has errors, trust the
  log, not the batch exit code.
- If the log timestamp does not change, investigate the Keil command line or
  project file encoding before making more source changes.
- If `.uvprojx` must be edited, keep changes focused and avoid whole-file
  reformatting.
- If source files contain Chinese comments, confirm encoding before rewriting.
- If the fix seems to require SDK edits, first try project-owned adapters,
  wrappers, configuration, or weak stubs for intentionally excluded optional
  components.
- If Keil silently fails after a project XML edit, check for BOM or encoding
  changes in `.uvprojx`.
- If multiple `.uvprojx` files exist, pass `-ProjectFile` explicitly.
- If multiple targets exist, pass `-TargetName` explicitly.
- If `UV4.exe` is not in PATH, pass `-Uv4Path C:\Keil_v5\UV4\UV4.exe`, set
  `KEIL_UV4_PATH`, or fill `UV4_EXE_FULL_PATH` in the generated
  `build_keil.bat`.
