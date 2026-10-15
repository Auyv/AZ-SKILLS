# Build Script Template Notes

The generator creates one self-contained Windows BAT for an app under either `code\task-mpu` or legacy `code\task-jpz_dev\mpu`. Set `-MpuRelativePath` for a non-standard workspace layout.

The BAT:

- uses the confirmed fixed Docker container ID without name lookup
- starts a stopped container and waits for readiness
- passes the build environment into the container and invokes `app/<AppName>/build/build.sh`
- writes the build output to `ai_tmp\mpu_debug_build`
- does not create a companion `.sh` or Docker CLI config directory

The single-BAT execution path was verified on 2026-10-08 with the non-standard workspace layout supplied through `-MpuRelativePath`. The `can_server_app` build completed with exit code 0 and produced an ARM ELF32 executable. Product-macro whitespace warnings were present; no compiler errors were reported.
