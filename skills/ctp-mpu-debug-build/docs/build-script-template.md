# Build Script Template Notes

The generated `.bat` follows the existing project entry pattern. The helper supports:

```text
code\task-mpu
code\task-jpz_dev\mpu (legacy)
```

It chooses the first supported root containing `app/<AppName>/build/build.sh`. A non-standard
workspace-relative root can be selected with `-MpuRelativePath`.

Key improvements for generated per-app scripts:

- resolve the Docker container by name first (`FOTON-G11`), then optional id
- start the container when it exists but is stopped
- use `docker exec -i` for automation-friendly execution
- run an app-specific shell script, avoiding edits to shared `az_build_linux_app.sh`
- keep build logs under `ai_tmp\mpu_debug_build`

The generated `.sh` exports:

- `G_PROJECT_NAME`
- `G_PRODUCT_NAME`
- `G_BUILD_MODEL`
- `BUILD_PROJECT_NAME`
- `BUILD_PRODUCT_NAME`
- `BUILD_PLATFORM_NAME`
- `STAGING_DIR`

Then it calls:

```bash
${SCRIPT_DIR}/app/${APP_NAME}/build/build.sh
```

and exits non-zero when that app build fails.
